#pragma once

#include <dunya/objectmodel/entity/entity.h>
#include <dunya/objectmodel/pose/pose.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

// Drives a run nobody is watching, and measures it.
//
// The schedule is frames rather than seconds, so the same shot lands on the
// same frame whatever the machine does. The frame loop asks it two questions -
// does this frame fire, and is the run over - and hands it what the frame cost;
// everything else about the run lives here, including the report. That is the
// same division FrameCheck already has: a harness drives and measures, and the
// loop only has to let it.
class DemoDriver {
public:
  // What one frame cost, split the way the loop measures it. The three phases
  // fail differently, so a frame that spiked has to say which one did it.
  struct Phases {
    float carveMs = 0.0f;
    float uploadMs = 0.0f;
    float physicsMs = 0.0f;

    // How much of the world was awake, and how many fixed steps the frame
    // bought. A physics cost is one or the other and the mean cannot say which
    // without them.
    uint32_t activeBodies = 0;
    uint32_t substeps = 0;
  };

  // What the scene held when the run ended. Handed in rather than read here,
  // because the world, the volume pool and the runtime are the loop's and a
  // harness that reached for all three would be back inside it.
  struct SceneSummary {
    size_t volumes = 0;
    size_t volumeCapacity = 0;
    size_t lattices = 0;
    size_t objects = 0;
    size_t residentBytes = 0;
    size_t shapes = 0;
  };

  // Zero frames means nobody scripted this run: every question below then
  // answers in the negative and nothing is recorded. That is also the default,
  // because the options that would say otherwise arrive after construction.
  explicit DemoDriver(uint32_t frames = 0u, float shotsPerSecond = 0.0f);

  [[nodiscard]] bool active() const noexcept;

  [[nodiscard]] bool finished(uint32_t frameIndex) const noexcept;

  // Whether this frame fires, and where. The target is a pair of fractions
  // across the wall's face rather than a world point, because where the wall is
  // belongs to the scene and not to the schedule.
  [[nodiscard]] bool fires(uint32_t frameIndex);

  [[nodiscard]] glm::vec2 target() const noexcept;

  // The frame that just ended. Craters arrive as a running total rather than a
  // delta, so the driver owns the subtraction and nobody else has to remember
  // to reset a counter.
  void record(
    uint32_t frameIndex,
    float realDt,
    uint32_t cratersApplied,
    const Phases& phases
  );

  // Whether the awake bodies are going anywhere. Jolt's "awake" is a velocity
  // threshold held over half a second, so a body whose contact set flickers
  // stays awake while its geometry sits still. The two cost the same and mean
  // opposite things, and only the pose can tell them apart.
  void measureMotion(const entt::registry& registry);

  void report(const SceneSummary& scene) const;

private:
  // Every frame the run measured, so it can report a distribution rather than
  // the last second's mean. A spike is the thing that matters and an average is
  // exactly what hides it - and the index comes with it, because "the worst
  // frame was 50 ms" and "the worst frame was the one that spawned a ball" are
  // different findings.
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

  // Frames between scripted shots. Sixty hertz is the reference, so a rate of
  // two shots a second is thirty frames.
  uint32_t m_interval = 240;

  // The R2 sequence's index, which is what spreads the shots across the wall
  // and what makes the same run do it the same way twice.
  uint32_t m_shotsFired = 0;

  glm::vec2 m_target{0.0f};
  bool m_firedThisFrame = false;

  // The running total as of the last recorded frame, so the next one can report
  // its own craters rather than everybody's.
  uint32_t m_cratersReported = 0;

  std::vector<Frame> m_measured;

  uint32_t m_movedBodies = 0;
  float m_maxMoveMm = 0.0f;
  float m_maxTurnDeg = 0.0f;

  // Last frame's pose per body, so this frame's can be compared against it.
  // Outlives a frame, which the three numbers above do not.
  std::unordered_map<uint32_t, dunya::objectmodel::Pose> m_posePrevious;
};
