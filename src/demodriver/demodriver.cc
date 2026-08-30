#include "demodriver.ih"

namespace {

constexpr uint32_t FIRST_SHOT = 80u;

constexpr uint32_t WARMUP = 20u;

constexpr float MOVED_METRES = 1.0e-5f;
constexpr float TURNED_RADIANS = 1.0e-5f;

}  // namespace

DemoDriver::DemoDriver(uint32_t frames, float shotsPerSecond)
    : m_frames(frames) {
  if (shotsPerSecond > 0.0f) {
    m_interval = std::max(1u, uint32_t(60.0f / shotsPerSecond + 0.5f));
  }
}

bool DemoDriver::active() const noexcept {
  return m_frames > 0u;
}

bool DemoDriver::finished(uint32_t frameIndex) const noexcept {
  return m_frames > 0u && frameIndex >= m_frames;
}

bool DemoDriver::fires(uint32_t frameIndex) {
  m_firedThisFrame = m_frames > 0u && frameIndex >= FIRST_SHOT
                     && (frameIndex - FIRST_SHOT) % m_interval == 0u;

  if (m_firedThisFrame) {
    m_target = glm::fract(
      static_cast<float>(m_shotsFired) * glm::vec2(0.7548777f, 0.5698403f)
    );

    ++m_shotsFired;
  }

  return m_firedThisFrame;
}

glm::vec2 DemoDriver::target() const noexcept {
  return m_target;
}

void DemoDriver::record(
  uint32_t frameIndex,
  float realDt,
  uint32_t cratersApplied,
  const Phases& phases
) {
  if (m_frames == 0u) {
    return;
  }

  if (frameIndex > WARMUP) {
    m_measured.push_back(
      {frameIndex - 1u,
       realDt * 1000.0f,
       cratersApplied - m_cratersReported,
       m_firedThisFrame,
       phases.carveMs,
       phases.uploadMs,
       phases.physicsMs,
       phases.activeBodies,
       phases.substeps,
       m_movedBodies,
       m_maxMoveMm,
       m_maxTurnDeg}
    );
  }

  m_cratersReported = cratersApplied;
}

void DemoDriver::measureMotion(const entt::registry& registry) {
  m_movedBodies = 0;
  m_maxMoveMm = 0.0f;
  m_maxTurnDeg = 0.0f;

  const auto simulated =
    registry.view<dunya::objectmodel::RigidBody, dunya::objectmodel::Pose>();

  for (const dunya::objectmodel::Entity entity : simulated) {
    const dunya::objectmodel::Pose& pose =
      simulated.get<dunya::objectmodel::Pose>(entity);

    const uint32_t key = static_cast<uint32_t>(entity);

    const auto found = m_posePrevious.find(key);

    if (found == m_posePrevious.end()) {
      m_posePrevious.emplace(key, pose);

      continue;
    }

    const float moved = glm::length(pose.position - found->second.position);

    const float aligned =
      std::min(1.0f, std::abs(glm::dot(pose.rotation, found->second.rotation)));

    const float turned = 2.0f * std::acos(aligned);

    if (moved > MOVED_METRES || turned > TURNED_RADIANS) {
      ++m_movedBodies;
    }

    m_maxMoveMm = std::max(m_maxMoveMm, moved * 1000.0f);
    m_maxTurnDeg = std::max(m_maxTurnDeg, glm::degrees(turned));

    found->second = pose;
  }
}

void DemoDriver::report(const SceneSummary& scene) const {
  if (m_measured.empty()) {
    return;
  }

  std::vector<Frame> sorted = m_measured;

  std::sort(sorted.begin(), sorted.end(), [](const Frame& a, const Frame& b) {
    return a.ms < b.ms;
  });

  const auto at = [&sorted](double fraction) {
    const size_t index = static_cast<size_t>(fraction * (sorted.size() - 1));
    return sorted[index].ms;
  };

  double total = 0.0;
  size_t overBudget = 0;

  for (const Frame& frame : m_measured) {
    total += frame.ms;

    if (frame.ms > 16.6f) {
      ++overBudget;
    }
  }

  std::cout << "\ndemo: " << m_measured.size() << " frames, "
            << m_cratersReported << " craters, " << scene.volumes << " of "
            << scene.volumeCapacity << " volumes, " << scene.lattices
            << " lattices over " << scene.objects << " objects ("
            << (scene.residentBytes / (1024 * 1024)) << " MB), " << scene.shapes
            << " shapes\n"
            << "  mean " << (total / double(m_measured.size()))
            << " ms  median " << at(0.5) << "  p99 " << at(0.99) << "  worst "
            << sorted.back().ms << "\n"
            << "  over 16.6 ms: " << overBudget << " ("
            << (100.0 * double(overBudget) / double(m_measured.size()))
            << "%)\n";

  double carveTotal = 0.0;
  double uploadTotal = 0.0;
  double physicsTotal = 0.0;
  double activeTotal = 0.0;
  double substepTotal = 0.0;
  uint32_t activeWorst = 0u;
  uint32_t activeQuietest = UINT32_MAX;
  double movedTotal = 0.0;
  double moveTotal = 0.0;
  uint32_t movedWorst = 0u;
  uint32_t movedQuietest = UINT32_MAX;
  float moveWorst = 0.0f;
  float turnWorst = 0.0f;

  for (const Frame& frame : m_measured) {
    carveTotal += frame.carveMs;
    uploadTotal += frame.uploadMs;
    physicsTotal += frame.physicsMs;
    activeTotal += frame.activeBodies;
    substepTotal += frame.substeps;
    activeWorst = std::max(activeWorst, frame.activeBodies);
    activeQuietest = std::min(activeQuietest, frame.activeBodies);
    movedTotal += frame.movedBodies;
    moveTotal += frame.maxMoveMm;
    movedWorst = std::max(movedWorst, frame.movedBodies);
    movedQuietest = std::min(movedQuietest, frame.movedBodies);
    moveWorst = std::max(moveWorst, frame.maxMoveMm);
    turnWorst = std::max(turnWorst, frame.maxTurnDeg);
  }

  const double frames = double(m_measured.size());

  std::cout << "  mean carve " << (carveTotal / frames) << "  upload "
            << (uploadTotal / frames) << "  physics " << (physicsTotal / frames)
            << "  rest "
            << ((total - carveTotal - uploadTotal - physicsTotal) / frames)
            << "\n";

  std::cout << "  awake: mean " << (activeTotal / frames) << "  worst "
            << activeWorst << "  quietest " << activeQuietest
            << ",  mean substeps " << (substepTotal / frames) << "\n";

  std::cout << "  moved: mean " << (movedTotal / frames) << "  worst "
            << movedWorst << "  quietest " << movedQuietest
            << ",  mean furthest " << (moveTotal / frames)
            << " mm,  worst furthest " << moveWorst << " mm,  worst turn "
            << turnWorst << " deg\n";

  std::cout << "  worst frames:\n";

  const size_t show = std::min<size_t>(6u, sorted.size());

  for (size_t i = 0; i != show; ++i) {
    const Frame& frame = sorted[sorted.size() - 1u - i];

    std::cout << "    frame " << frame.index << "  " << frame.ms
              << " ms   craters " << frame.craters << "  carve "
              << frame.carveMs << "  upload " << frame.uploadMs << "  physics "
              << frame.physicsMs << "  rest "
              << (frame.ms - frame.carveMs - frame.uploadMs - frame.physicsMs)
              << (frame.fired ? "  (spawned a ball)" : "") << "\n";
  }

  std::cout << std::flush;
}
