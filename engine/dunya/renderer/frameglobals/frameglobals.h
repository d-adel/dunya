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
  "MarchParams must match its std140 block in sdf-shader.frag"
);

struct SceneCounts {
  uint32_t sdfRecords;
};

static_assert(
  sizeof(SceneCounts) == 4,
  "SceneCounts must match its std140 block in sdf-shader.frag"
);

struct LightUniform {
  glm::vec4 direction;
};

static_assert(
  sizeof(LightUniform) == 16,
  "LightUniform must match its std140 block in the shaders"
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
    const SceneCounts& counts,
    const LightUniform& light
  );

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

private:
  dunya::gpu::DescriptorGroup m_group;
};

}
