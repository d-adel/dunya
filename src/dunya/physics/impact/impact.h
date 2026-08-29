#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace dunya::physics {

// One body's side of a collision, in world space.
//
// Both bodies of a manifold produce one of these, because Jolt sorts a contact
// pair by BodyID rather than by which body is interesting, and either or
// neither may be the one that deforms. Filtering is the consumer's job.
struct Impact {
  // The raw entt id, read from the body's user data. Not an Entity: this
  // header is reached from Jolt's worker threads and knows nothing about a
  // registry.
  uint32_t entity = 0u;

  // Where the manifold's strongest contact point sits.
  glm::vec3 point{0.0f};

  // The surface normal of *this* body at that point, pointing out of it.
  glm::vec3 outward{0.0f};

  // Total normal impulse over the manifold, kg m / s. Estimated before the
  // solver runs, which is the only time it can be had from a contact callback.
  float impulse = 0.0f;

  // How fast the two surfaces were closing along the normal, m / s.
  //
  // This is what separates an impact from a load, and impulse is not: a box
  // merely standing on the ground renews a contact worth its own weight times
  // the timestep, and a tall enough stack pushes that past any threshold a
  // fired ball would clear. Closing speed does not care how heavy the stack
  // is, so it stays a fixed number as the scene grows.
  float speed = 0.0f;
};

// Records collisions hard enough to be worth reacting to.
//
// Only OnContactAdded, deliberately: a box already resting on the ground
// renews its contact through OnContactPersisted, so taking the added ones
// alone filters out the entire standing wall without a single test on
// velocity. What is left is things that have just met.
//
// Jolt calls contact listeners from several worker threads with every body
// locked, so this only appends under a mutex and touches nothing else. The
// frame loop drains it after the step returns, which is also where D3 puts the
// deformation.
class ImpactListener : public JPH::ContactListener {
public:
  explicit ImpactListener(float threshold) noexcept;

  void OnContactAdded(
    const JPH::Body& body1,
    const JPH::Body& body2,
    const JPH::ContactManifold& manifold,
    JPH::ContactSettings& settings
  ) override;

  // Moves everything recorded since the last call into out, clearing it first.
  // Empty is the normal case: most frames have no new contacts at all.
  void drain(std::vector<Impact>& out);

  // Closing speed below which a contact is not worth recording, m / s. Read on
  // worker threads, so it is atomic rather than merely written between steps.
  void setThreshold(float speed) noexcept;
  [[nodiscard]] float threshold() const noexcept;

private:
  std::mutex m_mutex;
  std::vector<Impact> m_impacts;
  std::atomic<float> m_threshold;
};

}  // namespace dunya::physics
