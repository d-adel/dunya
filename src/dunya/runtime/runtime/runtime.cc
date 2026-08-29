#include "runtime.ih"

namespace dunya::runtime {

// The JoltLibrary reference is a lifetime requirement rather than data:
// PhysicsWorld allocates through the pointer RegisterDefaultAllocator installs.
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

void Runtime::refreshBody(objectmodel::Entity entity) {
  const entt::registry& registry = m_world.registry();

  const auto* field = registry.try_get<dunya::field::SampledField>(entity);

  // Not an error: bodies follow the field, and the field arrives on the first
  // frame after Play rather than at construction.
  if (field == nullptr) {
    return;
  }

  // Borrowed, not owned. The component is the owner, and it is what a rebake
  // replaces, which is why this function has to run again afterwards.
  setBodyShape(entity, JPH::ShapeRefC(new physics::FieldShape(*field)));
}

void Runtime::reshapeAfterDeform(
  objectmodel::Entity entity,
  const glm::uvec3& brickBegin,
  const glm::uvec3& brickEnd
) {
  const entt::registry& registry = m_world.registry();

  const auto* field = registry.try_get<dunya::field::SampledField>(entity);
  const auto* body = registry.try_get<objectmodel::RigidBody>(entity);

  if (field == nullptr || body == nullptr) {
    return;
  }

  const JPH::Shape* current =
    m_physicsWorld.bodies().GetShape(JPH::BodyID(body->id));

  // Only a shape over this same grid can be patched. Anything else - a ball
  // on the shared projectile shape, or a body that has not been built yet -
  // falls back to the full walk rather than reusing somebody else's bricks.
  const auto* shape = dynamic_cast<const physics::FieldShape*>(current);

  if (shape == nullptr || &shape->field() != field) {
    refreshBody(entity);
    return;
  }

  setBodyShape(
    entity,
    JPH::ShapeRefC(
      new physics::FieldShape(*field, *shape, brickBegin, brickEnd)
    )
  );
}

void Runtime::wake(const glm::vec3& minimum, const glm::vec3& maximum) {
  // Jolt's SetShape invalidates the contact cache of the body it changed and
  // nothing else, and it refuses to activate a static one at all. So a crater
  // opening under a box that has gone to sleep leaves it resting on geometry
  // that is no longer there - the lattice, the volume and the shape all show
  // the hole, and the box hangs over it.
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

  // Jolt asserts on a zero mass, and a field is allowed to hold nothing: carve
  // an object away and this is what is left of it. Only a moving body needs
  // one, so a static body on an empty field is no trouble at all.
  const bool hasMass = shape->GetMassProperties().mMass > 0.0f;

  JPH::BodyInterface& bodies = m_physicsWorld.bodies();

  if (const auto* body = registry.try_get<objectmodel::RigidBody>(entity)) {
    // Mass properties come from the field too, so they are recomputed with it.
    // Recomputed from the new geometry only when there is any: a rebake that
    // empties an object leaves the body its last mass rather than a zero one,
    // and nothing is left to collide with either way.
    bodies.SetShape(
      JPH::BodyID(body->id),
      shape,
      !isStatic && hasMass,
      isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate
    );

    // That call recomputed mass from the new geometry, which is wanted — a
    // carved body is lighter — and discarded any override, which is not.
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

  // Jolt's Quat takes w last, glm's constructor takes it first.
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

  // Not optional: the sweeps measured a body embedding itself in the ground
  // from one metre of travel per step upward under the discrete solver.
  settings.mMotionQuality = JPH::EMotionQuality::LinearCast;

  // The whole handle, version included: the index alone would let a recycled
  // entity inherit a stale body's transform.
  settings.mUserData = static_cast<JPH::uint64>(static_cast<uint32_t>(entity));

  const JPH::BodyID id = bodies.CreateAndAddBody(
    settings,
    isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate
  );

  if (id.IsInvalid()) {
    throw std::runtime_error("Runtime: a field entity got no body");
  }

  m_world.setRigidBody(entity, id.GetIndexAndSequenceNumber());

  // A body instantiated from an authored one brings its scale with it.
  applyMassScale(entity);

  m_broadPhaseStale = true;
}

void Runtime::launch(objectmodel::Entity entity, const glm::vec3& velocity) {
  const auto* body = m_world.registry().try_get<objectmodel::RigidBody>(entity);

  // Not an error: a body arrives a frame after the world does, so a key can
  // reach this before one exists.
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

  // Null on a static body, which has no mass to scale.
  JPH::MotionProperties* motion = lock.GetBody().GetMotionPropertiesUnchecked();

  if (motion == nullptr) {
    return;
  }

  motion->ScaleToMass(mass);

  // Recorded against what the shape says it weighs, so what survives a rebake
  // is the material rather than the number. A field with nothing solid in it
  // has no weight to be a multiple of, and asks for no scale.
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
    // Once per batch rather than per body: it rebuilds the whole tree.
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

    // Not an error: a body can outlive the entity that named it.
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

}  // namespace dunya::runtime
