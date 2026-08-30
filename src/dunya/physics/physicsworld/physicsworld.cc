#include "physicsworld.ih"

namespace {

constexpr uint MAX_BODIES = 65536;
constexpr uint NUM_BODY_MUTEXES = 0;
constexpr uint MAX_BODY_PAIRS = 65536;
constexpr uint MAX_CONTACT_CONSTRAINTS = 65536;

constexpr uint TEMP_ALLOCATOR_SIZE = 64 * 1024 * 1024;

constexpr float IMPACT_THRESHOLD = 3.0f;

uint workerCount() {
  const unsigned int count = std::thread::hardware_concurrency();
  return count > 1 ? count - 1 : 1;
}

}

namespace dunya::physics {

PhysicsWorld::PhysicsWorld()
    : m_tempAllocator(TEMP_ALLOCATOR_SIZE),
      m_jobSystem(cMaxPhysicsJobs, cMaxPhysicsBarriers, workerCount()),
      m_impacts(IMPACT_THRESHOLD) {
  m_system.Init(
    MAX_BODIES,
    NUM_BODY_MUTEXES,
    MAX_BODY_PAIRS,
    MAX_CONTACT_CONSTRAINTS,
    m_broadPhaseLayerInterface,
    m_objectVsBroadPhaseLayerFilter,
    m_objectLayerPairFilter
  );

  m_system.SetGravity(JPH::Vec3(0.0f, -GRAVITY, 0.0f));

  m_system.SetContactListener(&m_impacts);
}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::step() {
  const JPH::EPhysicsUpdateError error =
    m_system.Update(TIME_STEP, 1, &m_tempAllocator, &m_jobSystem);

  if (error != JPH::EPhysicsUpdateError::None && !m_updateErrorReported) {
    m_updateErrorReported = true;

    std::cout << "Physics update error, bitmask "
              << static_cast<uint32_t>(error)
              << " (1 = manifold cache full, 2 = body pair cache full, "
                 "4 = contact constraints full)\n";
  }
}

void PhysicsWorld::optimizeBroadPhase() {
  m_system.OptimizeBroadPhase();
}

BodyInterface& PhysicsWorld::bodies() {
  return m_system.GetBodyInterface();
}

const BodyInterface& PhysicsWorld::bodies() const {
  return m_system.GetBodyInterface();
}

PhysicsSystem& PhysicsWorld::system() {
  return m_system;
}

const PhysicsSystem& PhysicsWorld::system() const {
  return m_system;
}

ImpactListener& PhysicsWorld::impacts() noexcept {
  return m_impacts;
}

}
