#include "physicsworld.ih"

namespace {

constexpr uint MAX_BODIES = 65536;
constexpr uint NUM_BODY_MUTEXES = 0;
constexpr uint MAX_BODY_PAIRS = 65536;
constexpr uint MAX_CONTACT_CONSTRAINTS = 10240;

constexpr uint TEMP_ALLOCATOR_SIZE = 10 * 1024 * 1024;

// Closing speed below which a new contact is not an impact, in m / s. A stack
// finding its own rest closes at a few centimetres a second; anything thrown
// arrives an order of magnitude above this.
constexpr float IMPACT_THRESHOLD = 3.0f;

uint workerCount() {
  const unsigned int count = std::thread::hardware_concurrency();
  return count > 1 ? count - 1 : 1;
}

}  // namespace

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

  m_system.SetContactListener(&m_impacts);
}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::step() {
  m_system.Update(TIME_STEP, 1, &m_tempAllocator, &m_jobSystem);
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

}  // namespace dunya::physics
