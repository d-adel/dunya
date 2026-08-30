#include "impact.ih"

namespace dunya::physics {

ImpactListener::ImpactListener(float threshold) noexcept
    : m_threshold(threshold) {}

void ImpactListener::OnContactAdded(
  const JPH::Body& body1,
  const JPH::Body& body2,
  const JPH::ContactManifold& manifold,
  JPH::ContactSettings& settings
) {
  if (manifold.mRelativeContactPointsOn1.empty()) {
    return;
  }

  JPH::CollisionEstimationResult estimate;

  JPH::EstimateCollisionResponse(
    body1,
    body2,
    manifold,
    estimate,
    settings.mCombinedFriction,
    settings.mCombinedRestitution
  );

  float total = 0.0f;
  float strongest = 0.0f;
  uint32_t at = 0u;

  for (uint32_t i = 0; i != estimate.mContactImpulse.size(); ++i) {
    const float impulse = estimate.mContactImpulse[i];

    total += impulse;

    if (impulse > strongest) {
      strongest = impulse;
      at = i;
    }
  }

  const uint32_t point =
    std::min(at, uint32_t(manifold.mRelativeContactPointsOn1.size() - 1u));

  const JPH::RVec3 on1 = manifold.GetWorldSpaceContactPointOn1(point);
  const JPH::RVec3 on2 = manifold.GetWorldSpaceContactPointOn2(
    std::min(point, uint32_t(manifold.mRelativeContactPointsOn2.size() - 1u))
  );

  const JPH::Vec3 closing =
    body1.GetPointVelocity(on1) - body2.GetPointVelocity(on2);

  const float speed = closing.Dot(manifold.mWorldSpaceNormal);

  if (speed < m_threshold.load(std::memory_order_relaxed)) {
    return;
  }

  const glm::vec3 normal(
    manifold.mWorldSpaceNormal.GetX(),
    manifold.mWorldSpaceNormal.GetY(),
    manifold.mWorldSpaceNormal.GetZ()
  );

  const Impact first{
    static_cast<uint32_t>(body1.GetUserData()),
    glm::vec3(float(on1.GetX()), float(on1.GetY()), float(on1.GetZ())),
    normal,
    total,
    speed
  };

  const Impact second{
    static_cast<uint32_t>(body2.GetUserData()),
    glm::vec3(float(on2.GetX()), float(on2.GetY()), float(on2.GetZ())),
    -normal,
    total,
    speed
  };

  const std::lock_guard<std::mutex> guard(m_mutex);

  m_impacts.push_back(first);
  m_impacts.push_back(second);
}

void ImpactListener::drain(std::vector<Impact>& out) {
  const std::lock_guard<std::mutex> guard(m_mutex);

  out.clear();
  out.swap(m_impacts);
}

void ImpactListener::setThreshold(float speed) noexcept {
  m_threshold.store(speed, std::memory_order_relaxed);
}

float ImpactListener::threshold() const noexcept {
  return m_threshold.load(std::memory_order_relaxed);
}

}  // namespace dunya::physics
