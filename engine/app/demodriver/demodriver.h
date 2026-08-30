#pragma once

#include <dunya/core/telemetry/telemetry.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/component/pose/pose.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class DemoDriver {
public:
  explicit DemoDriver(uint32_t frames = 0u, float shotsPerSecond = 0.0f);

  [[nodiscard]] bool active() const noexcept;

  [[nodiscard]] bool finished(uint32_t frameIndex) const noexcept;

  [[nodiscard]] bool fires(uint32_t frameIndex);

  [[nodiscard]] glm::vec2 target() const noexcept;

  void record(uint32_t frameIndex, const dunya::core::Telemetry& telemetry);

  void measureMotion(
    const entt::registry& registry,
    dunya::core::Telemetry& telemetry
  );

  void report(const dunya::core::Telemetry& telemetry) const;

private:
  struct Frame {
    uint32_t index = 0;
    bool fired = false;
    std::vector<double> values;
  };

  [[nodiscard]] double at(
    const Frame& frame,
    dunya::core::Telemetry::Key key
  ) const;

  uint32_t m_frames = 0;
  uint32_t m_interval = 240;
  uint32_t m_shotsFired = 0;

  glm::vec2 m_target{0.0f};
  bool m_firedThisFrame = false;

  std::vector<Frame> m_measured;

  std::unordered_map<uint32_t, dunya::objectmodel::Pose> m_posePrevious;
};
