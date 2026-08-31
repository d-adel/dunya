#pragma once

#include <dunya/gpu/pipeline/pipeline.h>
#include <dunya/renderer/frameglobals/frameglobals.h>
#include <dunya/renderer/resourcetable/resourcetable.h>
#include <dunya/renderer/sdfrecordtable/sdfrecordtable.h>

#include <vulkan/vulkan.h>

#include <span>
#include <vector>

namespace dunya::renderer {

enum class BindGroup {
  FrameGlobals,
  Resources,
  SdfRecords
};

[[nodiscard]] std::span<const BindGroup> pipelineBindGroups(
  dunya::gpu::PipelineType type
);

[[nodiscard]] std::span<const BindGroup> bakeBindGroups();

[[nodiscard]] std::vector<VkDescriptorSetLayout> bindGroupLayouts(
  std::span<const BindGroup> groups,
  const FrameGlobals& frameGlobals,
  const ResourceTable& resources,
  const SdfRecordTable& records
);

[[nodiscard]] std::vector<VkDescriptorSetLayout> pipelineSetLayouts(
  dunya::gpu::PipelineType type,
  const FrameGlobals& frameGlobals,
  const ResourceTable& resources,
  const SdfRecordTable& records
);

}
