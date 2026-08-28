#pragma once

#include <dunya/gpu/descriptorgroup/descriptorgroup.h>
#include <dunya/gpu/device/device.h>

#include <glm/glm.hpp>

#include <cstdint>

namespace dunya::renderer {

struct CameraUniform {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewProj;
  glm::mat4 inverseViewProj;
  glm::vec4 position;
};

// The march's tunable numbers, as the field shader reads them; defaults from
// CMake. std140 rounds the 28 bytes to 32; the static_assert catches a reorder.
struct MarchParams {
  float epsilon;
  float maxDistance;
  float omega;
  float gradientEpsilon;

  float shadowMaxDistance;
  float shadowSharpness;
  uint32_t maxIterations;
};

static_assert(
  sizeof(MarchParams) == 28,
  "MarchParams must match its std140 block in field-shader.frag"
);

// How many of each thing this frame carries. Shaders read their loop bound from
// here rather than walking a table's capacity.
struct SceneCounts {
  uint32_t fieldRecords;
};

static_assert(
  sizeof(SceneCounts) == 4,
  "SceneCounts must match its std140 block in field-shader.frag"
);

class FrameGlobals {
public:
  explicit FrameGlobals(const dunya::gpu::Device& device);

  FrameGlobals(FrameGlobals const&) = delete;
  FrameGlobals& operator=(FrameGlobals const&) = delete;
  FrameGlobals(FrameGlobals&&) = delete;
  FrameGlobals& operator=(FrameGlobals&&) = delete;

  ~FrameGlobals() = default;

  void update(
    uint32_t frame,
    const CameraUniform& camera,
    const MarchParams& march,
    const SceneCounts& counts
  );

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

private:
  dunya::gpu::DescriptorGroup m_group;
};

}  // namespace dunya::renderer
