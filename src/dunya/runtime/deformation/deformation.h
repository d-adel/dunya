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

// Turns the contacts a step recorded into craters on whatever they struck that
// is Deformable, and keeps the collision shape following the geometry.
//
// It writes the lattice, re-shapes the body and wakes the neighbours - the
// three that have to happen together, because a lattice the collision shape
// does not follow is a hole things roll over. What it does NOT do is send the
// change to the GPU: this library has no Vulkan in it, so the changed regions
// go on a queue and whoever owns the copy drains it.
class Deformation {
public:
  // What damage a hit should do is a judgement about the demo rather than
  // something the physics derives.
  struct Damage {
    // Closing speed in m / s below which a contact leaves no mark at all. A
    // gate on speed rather than impulse, because a stack of heavy boxes
    // standing still pushes impulse past any fixed number.
    float threshold = 3.0f;

    // Metres of depth per kg m / s, then clamped. Linear because a crater that
    // grows without bound eats the object on the first hard shot.
    float depthPerImpulse = 0.0002f;
    float minimumDepth = 0.03f;
    float maximumDepth = 0.25f;

    // A crater is a spherical cap, so the cutter is wider than it is deep. At
    // one it would be a puncture.
    float radiusPerDepth = 2.5f;

    // Craters carved per frame. A ball entering a wall produces sixteen impacts
    // in one physics update, and sixteen at two and a half milliseconds is
    // three frames of work inside one. The rest wait, which costs nothing
    // visible at sixty hertz and is the difference between a steady demo and a
    // stutter.
    uint32_t perFrame = 3u;

    // And no wider than this much of the struck object's shortest side, so the
    // same shot leaves a dent in a wall and a crater in a floor instead of
    // removing a small object outright. Absolute metres cannot do this: 0.25 m
    // is a scratch on the ground and most of a box.
    float widestFraction = 0.25f;
  };

  // What was carved this frame. Recorded rather than printed: a library that
  // holds an iostream is a library deciding where its output goes, and the
  // caller is the one that knows whether anybody is reading.
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

  // D3: after the solve, never inside it. The contacts of every substep are
  // drained together, so a frame that stepped four times craters once per
  // impact rather than four times.
  void applyImpacts(Runtime& runtime);

  // Carves one cutter into one entity's lattice, re-shapes its body, wakes
  // whatever was resting on the region, and records the changed region.
  void carve(
    Runtime& runtime,
    objectmodel::Entity entity,
    const field::Primitive& cutter
  );

  // For a caller that wrote the lattice itself and needs the same region sent.
  // Merged with anything already queued for that entity.
  void markDirty(objectmodel::Entity entity, const field::SampleBox& box);

  // Entities whose lattice changed and the region of each that did, for
  // whoever owns the GPU copy.
  [[nodiscard]] std::span<
    const std::pair<objectmodel::Entity, field::SampleBox>>
  dirty() const noexcept;

  void clearDirty() noexcept;

  [[nodiscard]] std::span<const Crater> cratersThisFrame() const noexcept;

private:
  // An impact that has been accepted but not yet carved.
  //
  // Local space, and that is the whole reason this type exists rather than
  // deferring the Impact itself: a contact point is world space at the moment
  // of contact, and the body it belongs to has moved by the time a deferred
  // crater is carved. In the field's own frame it cannot go stale.
  struct PendingCrater {
    objectmodel::Entity entity{};
    glm::vec3 point{0.0f};
    glm::vec3 outward{0.0f};
    float impulse = 0.0f;
  };

  Damage m_damage;

  // Drained once per frame from the physics world. A member rather than a local
  // so the frame loop is not allocating a vector sixty times a second for the
  // frames - most of them - that record nothing.
  std::vector<physics::Impact> m_impacts;

  std::vector<PendingCrater> m_pending;
  std::vector<Crater> m_carved;

  std::vector<std::pair<objectmodel::Entity, field::SampleBox>> m_dirty;

  uint32_t m_cratersApplied = 0;
};

}  // namespace dunya::runtime
