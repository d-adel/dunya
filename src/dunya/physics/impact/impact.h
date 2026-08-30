#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace dunya::physics {

struct Impact {
  uint32_t entity = 0u;

  glm::vec3 point{0.0f};

  glm::vec3 outward{0.0f};

  float impulse = 0.0f;

  float speed = 0.0f;
};

class ImpactListener : public JPH::ContactListener {
public:
  explicit ImpactListener(float threshold) noexcept;

  void OnContactAdded(
    const JPH::Body& body1,
    const JPH::Body& body2,
    const JPH::ContactManifold& manifold,
    JPH::ContactSettings& settings
  ) override;

  void drain(std::vector<Impact>& out);

  void setThreshold(float speed) noexcept;
  [[nodiscard]] float threshold() const noexcept;

private:
  std::mutex m_mutex;
  std::vector<Impact> m_impacts;
  std::atomic<float> m_threshold;
};

}  // namespace dunya::physics
