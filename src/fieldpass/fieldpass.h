#pragma once

#include "computepipeline/computepipeline.h"
#include "fieldobjecttable/fieldobjecttable.h"
#include "device/device.h"
#include "field/field.h"
#include "field/sampled.h"
#include "field/analytic.h"
#include "texture/texture.h"
#include "fieldobject/fieldobject.h"
#include "volumepool/volumepool.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <span>

// Padded to vec4s so the push constant block needs no alignment reasoning.
struct BakeParams {
  glm::vec4 origin;
  glm::vec4 voxelSize;
  glm::uvec4 resolution;
  // x = which element of the volume arrays this dispatch writes.
  glm::uvec4 volume;
};

static_assert(
  sizeof(BakeParams) == 64,
  "BakeParams must match its push constant block in field-bake.comp"
);

class FieldPass {
public:
  FieldPass(
    const Device& device,
    const FieldObjectTable& table,
    const dunya::field::Aabb& box,
    const FieldObject& fieldObject
  );

  FieldPass(const FieldPass&) = delete;
  FieldPass& operator=(const FieldPass&) = delete;
  FieldPass(FieldPass&&) = delete;
  FieldPass& operator=(FieldPass&&) = delete;

  ~FieldPass() = default;

  // Re-runs the bake on the GPU when the primitives have changed. Reads the
  // pool out of the table, so it runs after that frame's pool write.
  void bakeIfDirty(
    uint32_t frame,
    uint32_t primitiveCount,
    uint32_t index,
    VolumeImages images
  );

  // Marks the volumes stale and re-fits the grid to what the primitives span.
  void primitivesChanged(const FieldObject& fieldObject);

  // Reads the baked volumes back and compares them against a CPU bake of the
  // same primitives. Slow and deliberate: a check, not part of a frame.
  void verifyBake(
    std::span<const dunya::field::Primitive> primitives,
    VolumeImages images
  );

private:
  const Device& m_device;

  // Baked once at construction. Declared before the volumes because they are
  // built from it, and members are constructed in declaration order.
  dunya::field::SampledField m_grid;

  const FieldObjectTable& m_table;
  ComputePipeline m_bakePipeline;

  // Starts dirty so the first frame re-bakes on the GPU over the CPU result,
  // which is what makes the two measurable against each other.
  bool m_gridDirty = true;
};
