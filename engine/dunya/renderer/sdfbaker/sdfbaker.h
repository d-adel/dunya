#pragma once

#include <dunya/gpu/computepipeline/computepipeline.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>
#include <dunya/renderer/sdfrecord/sdfrecord.h>
#include <dunya/renderer/volumepool/volumepool.h>
#include <dunya/gpu/device/device.h>

#include <glm/glm.hpp>

#include <cstddef>
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
  offsetof(BakeParams, origin) == 0,
  "BakeParams must match its push constant block in sdf-bake.comp"
);
static_assert(
  offsetof(BakeParams, voxelSize) == 16,
  "BakeParams must match its push constant block in sdf-bake.comp"
);
static_assert(
  offsetof(BakeParams, resolution) == 32,
  "BakeParams must match its push constant block in sdf-bake.comp"
);
static_assert(
  offsetof(BakeParams, volume) == 48,
  "BakeParams must match its push constant block in sdf-bake.comp"
);
static_assert(
  sizeof(BakeParams) == 64,
  "BakeParams must match its push constant block in sdf-bake.comp"
);

struct BakeShaders {
  const char* distanceSpirv;
  const char* lipschitzSpirv;
};

[[nodiscard]] BakeShaders bakeShaders();

[[nodiscard]] VkPushConstantRange bakePushConstantRange();

class SdfBaker {
public:
  SdfBaker(const dunya::gpu::Device& device, const SdfRecordTable& table);

  SdfBaker(const SdfBaker&) = delete;
  SdfBaker& operator=(const SdfBaker&) = delete;
  SdfBaker(SdfBaker&&) = delete;
  SdfBaker& operator=(SdfBaker&&) = delete;

  void bake(const SdfRecord& grid, uint32_t frame, VolumeImages images) const;

  void verifyBake(
    const dunya::objectmodel::SdfGrid& grid,
    uint32_t volumeIndex,
    std::span<const dunya::field::Primitive> primitives,
    VolumeImages images
  ) const;

private:
  const dunya::gpu::Device& m_device;
  const SdfRecordTable& m_table;

  dunya::gpu::ComputePipeline m_bakePipeline;

  dunya::gpu::ComputePipeline m_lipschitzPipeline;
};

}
