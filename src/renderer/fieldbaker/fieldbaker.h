#pragma once

#include "gpu/computepipeline/computepipeline.h"
#include "fieldobjecttable/fieldobjecttable.h"
#include "objectmodel/fieldobject/fieldobject.h"
#include "volumepool/volumepool.h"
#include "gpu/device/device.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <span>

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

class FieldBaker {
public:
  FieldBaker(const Device& device, const FieldObjectTable& table);

  FieldBaker(const FieldBaker&) = delete;
  FieldBaker& operator=(const FieldBaker&) = delete;
  FieldBaker(FieldBaker&&) = delete;
  FieldBaker& operator=(FieldBaker&&) = delete;

  void bake(
    const FieldObjectGPU& fieldObject,
    uint32_t frame,
    VolumeImages images
  ) const;

  void verifyBake(
    const FieldObject& fieldObject,
    std::span<const dunya::field::Primitive> primitives,
    VolumeImages images
  ) const;

private:
  const Device& m_device;
  const FieldObjectTable& m_table;

  ComputePipeline m_bakePipeline;
};
