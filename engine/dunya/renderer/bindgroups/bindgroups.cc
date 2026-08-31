#include "bindgroups.ih"

namespace dunya::renderer {

namespace {

constexpr std::array<BindGroup, 2> MESH_GROUPS{
  BindGroup::FrameGlobals,
  BindGroup::Resources
};

constexpr std::array<BindGroup, 3> SDF_GROUPS{
  BindGroup::FrameGlobals,
  BindGroup::Resources,
  BindGroup::SdfRecords
};

constexpr std::array<BindGroup, 1> GLOBALS_ONLY{BindGroup::FrameGlobals};

constexpr std::array<BindGroup, 1> RECORDS_ONLY{BindGroup::SdfRecords};

}

std::span<const BindGroup> pipelineBindGroups(dunya::gpu::PipelineType type) {
  switch (type) {
    case dunya::gpu::PipelineType::Mesh:
      return MESH_GROUPS;
    case dunya::gpu::PipelineType::Sdf:
      return SDF_GROUPS;
    case dunya::gpu::PipelineType::Grid:
    case dunya::gpu::PipelineType::Sky:
      return GLOBALS_ONLY;
  }

  return {};
}

std::span<const BindGroup> bakeBindGroups() {
  return RECORDS_ONLY;
}

std::vector<VkDescriptorSetLayout> bindGroupLayouts(
  std::span<const BindGroup> groups,
  const FrameGlobals& frameGlobals,
  const ResourceTable& resources,
  const SdfRecordTable& records
) {
  std::vector<VkDescriptorSetLayout> layouts;
  layouts.reserve(groups.size());

  for (const BindGroup group : groups) {
    switch (group) {
      case BindGroup::FrameGlobals:
        layouts.push_back(frameGlobals.setLayout());
        break;
      case BindGroup::Resources:
        layouts.push_back(resources.setLayout());
        break;
      case BindGroup::SdfRecords:
        layouts.push_back(records.setLayout());
        break;
    }
  }

  return layouts;
}

std::vector<VkDescriptorSetLayout> pipelineSetLayouts(
  dunya::gpu::PipelineType type,
  const FrameGlobals& frameGlobals,
  const ResourceTable& resources,
  const SdfRecordTable& records
) {
  return bindGroupLayouts(
    pipelineBindGroups(type),
    frameGlobals,
    resources,
    records
  );
}

}
