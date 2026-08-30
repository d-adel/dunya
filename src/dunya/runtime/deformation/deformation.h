#pragma once

#include <dunya/field/sampled/sampled.h>
#include <dunya/objectmodel/entity/entity.h>
#include <dunya/physics/impact/impact.h>
#include <dunya/runtime/runtime/runtime.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace dunya::runtime {

class Deformation {
public:
  struct Damage {
    float threshold = 3.0f;

    float depthPerImpulse = 0.0002f;
    float minimumDepth = 0.03f;
    float maximumDepth = 0.25f;

    float radiusPerDepth = 2.5f;

    uint32_t perFrame = 3u;

    float widestFraction = 0.25f;
  };

  struct Crater {
    objectmodel::Entity entity{};
    float impulse = 0.0f;
    float depth = 0.0f;
    float radius = 0.0f;
    float milliseconds = 0.0f;
  };

  [[nodiscard]] Damage& damage() noexcept;
  [[nodiscard]] const Damage& damage() const noexcept;

  [[nodiscard]] uint32_t cratersApplied() const noexcept;

  void applyImpacts(Runtime& runtime);

  void carve(
    Runtime& runtime,
    objectmodel::Entity entity,
    const field::Primitive& cutter
  );

  void markDirty(objectmodel::Entity entity, const field::SampleBox& box);

  [[nodiscard]] std::span<
    const std::pair<objectmodel::Entity, field::SampleBox>>
  dirty() const noexcept;

  void clearDirty() noexcept;

  [[nodiscard]] std::span<const Crater> cratersThisFrame() const noexcept;

private:
  struct PendingCrater {
    objectmodel::Entity entity{};
    glm::vec3 point{0.0f};
    glm::vec3 outward{0.0f};
    float impulse = 0.0f;
  };

  Damage m_damage;

  std::vector<physics::Impact> m_impacts;

  std::vector<PendingCrater> m_pending;
  std::vector<Crater> m_carved;

  std::vector<std::pair<objectmodel::Entity, field::SampleBox>> m_dirty;

  uint32_t m_cratersApplied = 0;
};

}
