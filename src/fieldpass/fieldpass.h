#pragma once

#include "computepipeline/computepipeline.h"
#include "descriptorgroup/descriptorgroup.h"
#include "device/device.h"
#include "field/field.h"
#include "field/sampled.h"
#include "texture/texture.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <span>

struct FieldFrame {
  // x = live primitives, y = which representation to evaluate,
  // z = how far the shader must scan to have seen every unbounded primitive
  glm::uvec4 config;

  // w is the slack baked around the contents, which is what lets a point
  // outside the grid be bounded away from everything inside it.
  glm::vec4 gridOrigin;

  glm::vec4 gridVoxelSize;
  glm::uvec4 gridResolution;
};

// Padded to vec4s so the push constant block needs no alignment reasoning.
struct BakeParams {
  glm::vec4 origin;
  glm::vec4 voxelSize;
  glm::uvec4 resolution;
};

static_assert(
  sizeof(BakeParams) == 48,
  "BakeParams must match its push constant block in field-bake.comp"
);

class FieldPass {
public:
  FieldPass(
    const Device& device,
    std::span<const dunya::field::Primitive> primitives
  );

  FieldPass(const FieldPass&) = delete;
  FieldPass& operator=(const FieldPass&) = delete;
  FieldPass(FieldPass&&) = delete;
  FieldPass& operator=(FieldPass&&) = delete;

  ~FieldPass() = default;

  // The grid's placement is the pass's own business, so callers supply only
  // what changes: how many primitives are live and which representation to use.
  void update(uint32_t frame, uint32_t primitiveCount, uint32_t representation);

  // Re-runs the bake on the GPU when the primitives have changed. Called once
  // per frame after the staged primitive write has reached this frame's copy,
  // because the dispatch reads that copy.
  void bakeIfDirty(uint32_t frame, uint32_t primitiveCount);

  // Stages the whole array. The group carries it into each frame's own copy,
  // so this is safe to call while frames are in flight.
  void uploadPrimitives(std::span<const dunya::field::Primitive> primitives);

  // Reads the baked volumes back and compares them against a CPU bake of the
  // same primitives. Slow and deliberate: a check, not part of a frame.
  void verifyBake(std::span<const dunya::field::Primitive> primitives);

  const VkDescriptorSetLayout& setLayout() const noexcept;
  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;

private:
  const Device& m_device;

  // Baked once at construction. Declared before the volumes because they are
  // built from it, and members are constructed in declaration order.
  dunya::field::SampledField m_grid;

  Texture m_distanceVolume;
  Texture m_materialVolume;

  DescriptorGroup m_group;

  // After the group, because it needs the set layout the group owns.
  ComputePipeline m_bakePipeline;

  // Starts dirty so the first frame re-bakes on the GPU over the CPU result,
  // which is what makes the two measurable against each other.
  bool m_gridDirty = true;

  // One past the last unbounded primitive. Outside the grid only those exist,
  // and edits append bounded ones, so this stays small while the array grows.
  uint32_t m_unboundedScan = 0;
};
