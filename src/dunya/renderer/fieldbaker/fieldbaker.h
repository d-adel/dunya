#pragma once

#include <dunya/gpu/computepipeline/computepipeline.h>
#include <dunya/renderer/fieldrecordtable/fieldrecordtable.h>
#include <dunya/renderer/fieldrecord/fieldrecord.h>
#include <dunya/renderer/volumepool/volumepool.h>
#include <dunya/gpu/device/device.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>

namespace dunya::renderer {

struct BakeParams {
  glm::vec4 origin;
  glm::vec4 voxelSize;
  glm::uvec4 resolution;

  glm::uvec4 volume;
};

static_assert(
  sizeof(BakeParams) == 64,
  "BakeParams must match its push constant block in field-bake.comp"
);

class FieldBaker {
public:
  FieldBaker(const dunya::gpu::Device& device, const FieldRecordTable& table);

  FieldBaker(const FieldBaker&) = delete;
  FieldBaker& operator=(const FieldBaker&) = delete;
  FieldBaker(FieldBaker&&) = delete;
  FieldBaker& operator=(FieldBaker&&) = delete;

  void bake(const FieldRecord& grid, uint32_t frame, VolumeImages images) const;

  void verifyBake(
    const dunya::objectmodel::SdfGrid& grid,
    uint32_t volumeIndex,
    std::span<const dunya::field::Primitive> primitives,
    VolumeImages images
  ) const;

private:
  const dunya::gpu::Device& m_device;
  const FieldRecordTable& m_table;

  dunya::gpu::ComputePipeline m_bakePipeline;

  dunya::gpu::ComputePipeline m_lipschitzPipeline;
};

}  // namespace dunya::renderer
