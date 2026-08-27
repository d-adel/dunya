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

/* The march's tunable numbers, as the field shader reads them.
 *
 * These used to be compile-time defines shared with GLSL, which is right for
 * anything that sizes an array and wrong for anything you would want to *turn*.
 * Tuning by rebuild is not tuning. Idiom 27 is unharmed: it has gone from one
 * number in two toolchains to one number in one buffer, which is the same
 * principle with fewer copies.
 *
 * Their defaults still come from CMake, so the value that ships is written down
 * in exactly one place.
 *
 * All scalars, so std140 packs them consecutively at their natural offsets.
 * Seven of them is 28 bytes and std140 rounds the block itself up to 32, so the
 * binding reserves that; the four trailing bytes are padding nothing reads. The
 * static_assert is what catches a member being added or reordered.
 */
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

class FrameGlobals {
public:
  FrameGlobals(const dunya::gpu::Device& device);

  FrameGlobals(FrameGlobals const&) = delete;
  FrameGlobals& operator=(FrameGlobals const&) = delete;
  FrameGlobals(FrameGlobals&&) = delete;
  FrameGlobals& operator=(FrameGlobals&&) = delete;

  ~FrameGlobals() = default;

  void update(
    uint32_t frame,
    const CameraUniform& camera,
    const MarchParams& march
  );

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

private:
  dunya::gpu::DescriptorGroup m_group;
};

}  // namespace dunya::renderer
