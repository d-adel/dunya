#include "runtime.ih"

namespace dunya::runtime {

namespace {

// Below this the grid box counts as centred on the entity origin and the
// offset wrapper is skipped.
constexpr float CENTRE_EPSILON = 1e-6f;

}  // namespace

// The JoltLibrary reference is a lifetime requirement rather than data:
// PhysicsWorld allocates through the pointer RegisterDefaultAllocator installs.
Runtime::Runtime(const objectmodel::World& source, physics::JoltLibrary&) {
  objectmodel::instantiateWorld(source, m_world);
  createBodies();
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

void Runtime::step() {
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

void Runtime::createBodies() {
  JPH::BodyInterface& bodies = m_physicsWorld.bodies();
  const entt::registry& registry = m_world.registry();

  for (const objectmodel::Entity entity : m_world.fields()) {
    const bool isStatic = registry.all_of<objectmodel::StaticBody>(entity);

    const dunya::field::Aabb box =
      objectmodel::gridBox(m_world.primitives(entity));

    const glm::vec3 half = (box.maximum - box.minimum) * 0.5f;
    const glm::vec3 centre = (box.maximum + box.minimum) * 0.5f;

    JPH::BoxShapeSettings boxSettings(JPH::Vec3(half.x, half.y, half.z));
    boxSettings.SetEmbedded();

    const JPH::ShapeSettings::ShapeResult boxShape = boxSettings.Create();

    if (boxShape.HasError()) {
      throw std::runtime_error(boxShape.GetError().c_str());
    }

    // A BoxShape is centred on the body origin, but the grid box need not be:
    // the ground's primitive sits half a unit below it. Carry the offset.
    JPH::ShapeRefC shape = boxShape.Get();

    if (glm::length(centre) > CENTRE_EPSILON) {
      JPH::RotatedTranslatedShapeSettings offsetSettings(
        JPH::Vec3(centre.x, centre.y, centre.z),
        JPH::Quat::sIdentity(),
        shape
      );
      offsetSettings.SetEmbedded();

      const JPH::ShapeSettings::ShapeResult offsetShape =
        offsetSettings.Create();

      if (offsetShape.HasError()) {
        throw std::runtime_error(offsetShape.GetError().c_str());
      }

      shape = offsetShape.Get();
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
      isStatic ? physics::ObjectLayers::NON_MOVING
               : physics::ObjectLayers::MOVING
    );

    // The whole handle, version included: the index alone would let a recycled
    // entity inherit a stale body's transform.
    settings.mUserData =
      static_cast<JPH::uint64>(static_cast<uint32_t>(entity));

    const JPH::BodyID id = bodies.CreateAndAddBody(
      settings,
      isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate
    );

    if (id.IsInvalid()) {
      throw std::runtime_error("Runtime: a field entity got no body");
    }

    m_world.setRigidBody(entity, id.GetIndexAndSequenceNumber());
  }

  // Once, after every body is in: it rebuilds the broad phase tree.
  m_physicsWorld.optimizeBroadPhase();
}

}  // namespace dunya::runtime
