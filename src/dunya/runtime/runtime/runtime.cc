#include "runtime.ih"

namespace dunya::runtime {

Runtime::Runtime(const objectmodel::World& source, physics::JoltLibrary&) {
  objectmodel::instantiateWorld(source, m_world);
}

objectmodel::World& Runtime::world() noexcept {
  return m_world;
}

const objectmodel::World& Runtime::world() const noexcept {
  return m_world;
}

physics::PhysicsWorld& Runtime::physics() noexcept {
  return m_physicsWorld;
}

JPH::ShapeRefC Runtime::shapeFor(const objectmodel::SharedField& held) {
  const dunya::field::SampledField* key = held.field.get();

  const auto found = m_shapes.find(key);

  if (found != m_shapes.end()) {
    if (!found->second.lattice.expired()) {
      return found->second.shape;
    }

    m_shapes.erase(found);
  }

  JPH::ShapeRefC shape(new physics::FieldShape(*held.field));

  rememberShape(held, shape);

  return shape;
}

void Runtime::rememberShape(
  const objectmodel::SharedField& held,
  const JPH::ShapeRefC& shape
) {
  std::erase_if(m_shapes, [](const auto& entry) {
    return entry.second.lattice.expired();
  });

  m_shapes.insert_or_assign(held.field.get(), SharedShape{held.field, shape});
}

void Runtime::refreshBody(objectmodel::Entity entity) {
  const auto* held =
    m_world.registry().try_get<objectmodel::SharedField>(entity);

  if (held == nullptr) {
    return;
  }

  setBodyShape(entity, shapeFor(*held));
}

void Runtime::reshapeAfterDeform(
  objectmodel::Entity entity,
  const glm::uvec3& brickBegin,
  const glm::uvec3& brickEnd
) {
  const entt::registry& registry = m_world.registry();

  const auto* held = registry.try_get<objectmodel::SharedField>(entity);
  const auto* body = registry.try_get<objectmodel::RigidBody>(entity);

  if (held == nullptr || body == nullptr) {
    return;
  }

  const dunya::field::SampledField* field = held->field.get();

  const JPH::Shape* current =
    m_physicsWorld.bodies().GetShape(JPH::BodyID(body->id));

  const auto* shape = dynamic_cast<const physics::FieldShape*>(current);

  if (shape == nullptr || &shape->field() != field) {
    refreshBody(entity);
    return;
  }

  const JPH::ShapeRefC reshaped(
    new physics::FieldShape(*field, *shape, brickBegin, brickEnd)
  );

  rememberShape(*held, reshaped);

  setBodyShape(entity, reshaped);
}

size_t Runtime::shapeCount() const noexcept {
  return m_shapes.size();
}

void Runtime::wake(const glm::vec3& minimum, const glm::vec3& maximum) {
  m_physicsWorld.bodies().ActivateBodiesInAABox(
    JPH::AABox(
      JPH::Vec3(minimum.x, minimum.y, minimum.z),
      JPH::Vec3(maximum.x, maximum.y, maximum.z)
    ),
    {},
    {}
  );
}

void Runtime::setBodyShape(
  objectmodel::Entity entity,
  const JPH::ShapeRefC& shape
) {
  const entt::registry& registry = m_world.registry();

  const bool isStatic = registry.all_of<objectmodel::StaticBody>(entity);

  const bool hasMass = shape->GetMassProperties().mMass > 0.0f;

  JPH::BodyInterface& bodies = m_physicsWorld.bodies();

  if (const auto* body = registry.try_get<objectmodel::RigidBody>(entity)) {
    bodies.SetShape(
      JPH::BodyID(body->id),
      shape,
      !isStatic && hasMass,
      isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate
    );

    applyMassScale(entity);

    return;
  }

  if (!isStatic && !hasMass) {
    if (!m_masslessReported) {
      m_masslessReported = true;
      std::cout << "A field with nothing solid in it gets no body" << std::endl;
    }

    return;
  }

  const objectmodel::Pose& pose = registry.get<objectmodel::Pose>(entity);

  JPH::BodyCreationSettings settings(
    shape,
    JPH::RVec3(pose.position.x, pose.position.y, pose.position.z),
    JPH::Quat(
      pose.rotation.x,
      pose.rotation.y,
      pose.rotation.z,
      pose.rotation.w
    ),
    isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
    isStatic ? physics::ObjectLayers::NON_MOVING : physics::ObjectLayers::MOVING
  );

  settings.mMotionQuality = JPH::EMotionQuality::LinearCast;

  settings.mUserData = static_cast<JPH::uint64>(static_cast<uint32_t>(entity));

  const JPH::BodyID id = bodies.CreateAndAddBody(
    settings,
    isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate
  );

  if (id.IsInvalid()) {
    throw std::runtime_error("Runtime: a field entity got no body");
  }

  m_world.setRigidBody(entity, id.GetIndexAndSequenceNumber());

  applyMassScale(entity);

  m_broadPhaseStale = true;
}

void Runtime::launch(objectmodel::Entity entity, const glm::vec3& velocity) {
  const auto* body = m_world.registry().try_get<objectmodel::RigidBody>(entity);

  if (body == nullptr) {
    return;
  }

  const JPH::BodyID id(body->id);

  m_physicsWorld.bodies().SetLinearVelocity(
    id,
    JPH::Vec3(velocity.x, velocity.y, velocity.z)
  );
  m_physicsWorld.bodies().ActivateBody(id);
}

void Runtime::setMass(objectmodel::Entity entity, float mass) {
  const auto* body = m_world.registry().try_get<objectmodel::RigidBody>(entity);

  if (body == nullptr || mass <= 0.0f) {
    return;
  }

  JPH::BodyLockWrite lock(
    m_physicsWorld.system().GetBodyLockInterface(),
    JPH::BodyID(body->id)
  );

  if (!lock.Succeeded()) {
    return;
  }

  JPH::MotionProperties* motion = lock.GetBody().GetMotionPropertiesUnchecked();

  if (motion == nullptr) {
    return;
  }

  motion->ScaleToMass(mass);

  const float derived = lock.GetBody().GetShape()->GetMassProperties().mMass;

  if (derived > 0.0f) {
    m_world.emplaceOrReplace<objectmodel::MassScale>(entity, {mass / derived});
  }
}

void Runtime::applyMassScale(objectmodel::Entity entity) {
  const entt::registry& registry = m_world.registry();

  const auto* scale = registry.try_get<objectmodel::MassScale>(entity);
  const auto* body = registry.try_get<objectmodel::RigidBody>(entity);

  if (scale == nullptr || body == nullptr) {
    return;
  }

  JPH::BodyLockWrite lock(
    m_physicsWorld.system().GetBodyLockInterface(),
    JPH::BodyID(body->id)
  );

  if (!lock.Succeeded()) {
    return;
  }

  JPH::MotionProperties* motion = lock.GetBody().GetMotionPropertiesUnchecked();
  const float derived = lock.GetBody().GetShape()->GetMassProperties().mMass;

  if (motion != nullptr && derived > 0.0f) {
    motion->ScaleToMass(derived * scale->factor);
  }
}

bool Runtime::despawn(objectmodel::Entity entity) {
  if (
    const auto* body =
      m_world.registry().try_get<objectmodel::RigidBody>(entity)
  ) {
    const JPH::BodyID id(body->id);

    m_physicsWorld.bodies().RemoveBody(id);
    m_physicsWorld.bodies().DestroyBody(id);
  }

  return m_world.destroyField(entity);
}

void Runtime::step() {
  if (m_broadPhaseStale) {
    m_physicsWorld.optimizeBroadPhase();
    m_broadPhaseStale = false;
  }

  m_physicsWorld.step();
}

void Runtime::syncPoses() {
  JPH::BodyIDVector active;
  m_physicsWorld.system().GetActiveBodies(JPH::EBodyType::RigidBody, active);

  if (active.empty()) {
    return;
  }

  const JPH::BodyInterface& bodies = m_physicsWorld.bodies();
  const entt::registry& registry = m_world.registry();

  m_poseScratch.clear();
  m_poseScratch.reserve(active.size());

  for (const JPH::BodyID& id : active) {
    const objectmodel::Entity entity{
      static_cast<uint32_t>(bodies.GetUserData(id))
    };

    if (!registry.valid(entity)) {
      continue;
    }

    JPH::RVec3 position;
    JPH::Quat rotation;
    bodies.GetPositionAndRotation(id, position, rotation);

    m_poseScratch.push_back(
      {entity,
       objectmodel::Pose{
         glm::vec3(position.GetX(), position.GetY(), position.GetZ()),
         glm::quat(
           rotation.GetW(),
           rotation.GetX(),
           rotation.GetY(),
           rotation.GetZ()
         )
       }}
    );
  }

  m_world.replaceMany<objectmodel::Pose>(m_poseScratch);
}

}
