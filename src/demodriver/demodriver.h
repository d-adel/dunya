#pragma once

#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/pose/pose.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

class DemoDriver {
public:
  struct Phases {
    float carveMs = 0.0f;
    float uploadMs = 0.0f;
    float physicsMs = 0.0f;

    uint32_t activeBodies = 0;
    uint32_t substeps = 0;
  };

  struct SceneSummary {
    size_t volumes = 0;
    size_t volumeCapacity = 0;
    size_t lattices = 0;
    size_t objects = 0;
    size_t residentBytes = 0;
    size_t shapes = 0;
  };

  explicit DemoDriver(uint32_t frames = 0u, float shotsPerSecond = 0.0f);

  [[nodiscard]] bool active() const noexcept;

  [[nodiscard]] bool finished(uint32_t frameIndex) const noexcept;

  [[nodiscard]] bool fires(uint32_t frameIndex);

  [[nodiscard]] glm::vec2 target() const noexcept;

  void record(
    uint32_t frameIndex,
    float realDt,
    uint32_t cratersApplied,
    const Phases& phases
  );

  void measureMotion(const entt::registry& registry);

  void report(const SceneSummary& scene) const;

private:
  struct Frame {
    uint32_t index = 0;
    float ms = 0.0f;
    uint32_t craters = 0;
    bool fired = false;

    float carveMs = 0.0f;
    float uploadMs = 0.0f;
    float physicsMs = 0.0f;

    uint32_t activeBodies = 0;
    uint32_t substeps = 0;

    uint32_t movedBodies = 0;
    float maxMoveMm = 0.0f;
    float maxTurnDeg = 0.0f;
  };

  uint32_t m_frames = 0;

  uint32_t m_interval = 240;

  uint32_t m_shotsFired = 0;

  glm::vec2 m_target{0.0f};
  bool m_firedThisFrame = false;

  uint32_t m_cratersReported = 0;

  std::vector<Frame> m_measured;

  uint32_t m_movedBodies = 0;
  float m_maxMoveMm = 0.0f;
  float m_maxTurnDeg = 0.0f;

  std::unordered_map<uint32_t, dunya::objectmodel::Pose> m_posePrevious;
};
