#include "physicsdemo.ih"

namespace {

const JPH::Vec3 FLOOR_HALF_EXTENTS{10.0f, 0.5f, 10.0f};

const JPH::RVec3 FLOOR_POSITION{0.0f, -0.5f, 0.0f};

constexpr float SPHERE_RADIUS = 0.5f;

const JPH::RVec3 SPHERE_POSITION{0.0f, 5.0f, 0.0f};

}  // namespace

PhysicsDemo::PhysicsDemo(dunya::physics::PhysicsWorld& world) : m_world(world) {
  JPH::BodyInterface& bodies = m_world.bodies();

  JPH::BoxShapeSettings floorShapeSettings(FLOOR_HALF_EXTENTS);
  floorShapeSettings.SetEmbedded();

  JPH::ShapeSettings::ShapeResult floorShapeResult =
    floorShapeSettings.Create();

  if (floorShapeResult.HasError()) {
    throw std::runtime_error(floorShapeResult.GetError().c_str());
  }

  JPH::BodyCreationSettings floorSettings(
    floorShapeResult.Get(),
    FLOOR_POSITION,
    JPH::Quat::sIdentity(),
    JPH::EMotionType::Static,
    dunya::physics::ObjectLayers::NON_MOVING
  );

  m_floorId =
    bodies.CreateAndAddBody(floorSettings, JPH::EActivation::DontActivate);

  if (m_floorId.IsInvalid()) {
    throw std::runtime_error("Failed to create floor body");
  }

  JPH::BodyCreationSettings sphereSettings(
    new JPH::SphereShape(SPHERE_RADIUS),
    SPHERE_POSITION,
    JPH::Quat::sIdentity(),
    JPH::EMotionType::Dynamic,
    dunya::physics::ObjectLayers::MOVING
  );

  m_sphereId =
    bodies.CreateAndAddBody(sphereSettings, JPH::EActivation::Activate);

  if (m_sphereId.IsInvalid()) {
    bodies.RemoveBody(m_floorId);
    bodies.DestroyBody(m_floorId);
    m_floorId = {};

    throw std::runtime_error("Failed to create sphere body");
  }

  m_world.optimizeBroadPhase();
}

PhysicsDemo::~PhysicsDemo() {
  JPH::BodyInterface& bodies = m_world.bodies();

  if (!m_sphereId.IsInvalid()) {
    bodies.RemoveBody(m_sphereId);
    bodies.DestroyBody(m_sphereId);
  }

  if (!m_floorId.IsInvalid()) {
    bodies.RemoveBody(m_floorId);
    bodies.DestroyBody(m_floorId);
  }
}

void PhysicsDemo::log() const {
  const JPH::BodyInterface& bodies = m_world.bodies();

  const JPH::RVec3 position = bodies.GetCenterOfMassPosition(m_sphereId);

  const JPH::Vec3 velocity = bodies.GetLinearVelocity(m_sphereId);

  std::printf(
    "sphere y=%.6f vy=%.6f\n",
    static_cast<double>(position.GetY()),
    static_cast<double>(velocity.GetY())
  );
}
