#include "demodriver.ih"

namespace {

constexpr uint32_t FIRST_SHOT = 80u;

constexpr uint32_t WARMUP = 20u;

constexpr float MOVED_METRES = 1.0e-5f;
constexpr float TURNED_RADIANS = 1.0e-5f;

struct Summary {
  double total = 0.0;
  double worst = 0.0;
  double quietest = 0.0;
  double mean = 0.0;
};

}

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
  const dunya::core::Telemetry& telemetry
) {
  if (m_frames == 0u || frameIndex <= WARMUP) {
    return;
  }

  Frame frame{};

  frame.index = frameIndex - 1u;
  frame.fired = m_firedThisFrame;
  frame.values.assign(telemetry.values().begin(), telemetry.values().end());

  m_measured.push_back(std::move(frame));
}

double DemoDriver::at(
  const Frame& frame,
  dunya::core::Telemetry::Key key
) const {
  return key < frame.values.size() ? frame.values[key] : 0.0;
}

void DemoDriver::measureMotion(
  const entt::registry& registry,
  dunya::core::Telemetry& telemetry
) {
  const auto moved = telemetry.key("moved");
  const auto furthest = telemetry.key("furthest");
  const auto turn = telemetry.key("turn");

  const auto simulated =
    registry.view<dunya::objectmodel::RigidBody, dunya::objectmodel::Pose>();

  for (const dunya::objectmodel::Entity entity : simulated) {
    const dunya::objectmodel::Pose& pose =
      simulated.get<dunya::objectmodel::Pose>(entity);

    const uint32_t slot = static_cast<uint32_t>(entity);

    const auto found = m_posePrevious.find(slot);

    if (found == m_posePrevious.end()) {
      m_posePrevious.emplace(slot, pose);

      continue;
    }

    const float distance = glm::length(pose.position - found->second.position);

    const float aligned =
      std::min(1.0f, std::abs(glm::dot(pose.rotation, found->second.rotation)));

    const float turned = 2.0f * std::acos(aligned);

    if (distance > MOVED_METRES || turned > TURNED_RADIANS) {
      telemetry.add(moved, 1.0);
    }

    telemetry.max(furthest, double(distance) * 1000.0);
    telemetry.max(turn, double(glm::degrees(turned)));

    found->second = pose;
  }
}

void DemoDriver::report(const dunya::core::Telemetry& telemetry) const {
  if (m_measured.empty()) {
    return;
  }

  const auto channel = [&telemetry](std::string_view name) {
    return telemetry.find(name);
  };

  const auto frameMs = channel("frame");
  const auto carveMs = channel("carve");
  const auto uploadMs = channel("upload");
  const auto physicsMs = channel("physics");
  const auto craterCount = channel("craters");

  std::vector<const Frame*> sorted;
  sorted.reserve(m_measured.size());

  for (const Frame& frame : m_measured) {
    sorted.push_back(&frame);
  }

  std::sort(sorted.begin(), sorted.end(), [&](const Frame* a, const Frame* b) {
    return at(*a, frameMs) < at(*b, frameMs);
  });

  const auto percentile = [&](double fraction) {
    const size_t index =
      static_cast<size_t>(fraction * double(sorted.size() - 1));

    return at(*sorted[index], frameMs);
  };

  const auto summarise = [&](dunya::core::Telemetry::Key key) {
    Summary out{};
    out.quietest = std::numeric_limits<double>::max();

    for (const Frame& frame : m_measured) {
      const double value = at(frame, key);

      out.total += value;
      out.worst = std::max(out.worst, value);
      out.quietest = std::min(out.quietest, value);
    }

    out.mean = out.total / double(m_measured.size());

    return out;
  };

  const Summary frames = summarise(frameMs);
  const Summary carve = summarise(carveMs);
  const Summary upload = summarise(uploadMs);
  const Summary physics = summarise(physicsMs);
  const Summary awake = summarise(channel("awake"));
  const Summary substeps = summarise(channel("substeps"));
  const Summary moved = summarise(channel("moved"));
  const Summary furthest = summarise(channel("furthest"));
  const Summary turn = summarise(channel("turn"));
  const Summary craters = summarise(craterCount);

  size_t overBudget = 0;

  for (const Frame& frame : m_measured) {
    if (at(frame, frameMs) > 16.6) {
      ++overBudget;
    }
  }

  const auto scene = [&telemetry](std::string_view name) {
    return telemetry.get(telemetry.find(name));
  };

  std::cout << "\ndemo: " << m_measured.size() << " frames, " << craters.total
            << " craters, " << scene("volumes") << " of "
            << scene("volumeCapacity") << " volumes, " << scene("lattices")
            << " lattices over " << scene("objects") << " objects ("
            << scene("residentMB") << " MB), " << scene("shapes") << " shapes\n"
            << "  mean " << frames.mean << " ms  median " << percentile(0.5)
            << "  p99 " << percentile(0.99) << "  worst " << frames.worst
            << "\n"
            << "  over 16.6 ms: " << overBudget << " ("
            << (100.0 * double(overBudget) / double(m_measured.size()))
            << "%)\n";

  std::cout << "  mean carve " << carve.mean << "  upload " << upload.mean
            << "  physics " << physics.mean << "  rest "
            << (frames.mean - carve.mean - upload.mean - physics.mean) << "\n";

  std::cout << "  awake: mean " << awake.mean << "  worst " << awake.worst
            << "  quietest " << awake.quietest << ",  mean substeps "
            << substeps.mean << "\n";

  std::cout << "  moved: mean " << moved.mean << "  worst " << moved.worst
            << "  quietest " << moved.quietest << ",  mean furthest "
            << furthest.mean << " mm,  worst furthest " << furthest.worst
            << " mm,  worst turn " << turn.worst << " deg\n";

  std::cout << "  worst frames:\n";

  const size_t show = std::min<size_t>(6u, sorted.size());

  for (size_t i = 0; i != show; ++i) {
    const Frame& frame = *sorted[sorted.size() - 1u - i];

    const double ms = at(frame, frameMs);
    const double frameCarve = at(frame, carveMs);
    const double frameUpload = at(frame, uploadMs);
    const double framePhysics = at(frame, physicsMs);

    std::cout << "    frame " << frame.index << "  " << ms << " ms   craters "
              << at(frame, craterCount) << "  carve " << frameCarve
              << "  upload " << frameUpload << "  physics " << framePhysics
              << "  rest " << (ms - frameCarve - frameUpload - framePhysics)
              << (frame.fired ? "  (spawned a ball)" : "") << "\n";
  }

  std::cout << std::flush;
}
