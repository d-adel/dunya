#pragma once

#include <dunya/gpu/descriptorgroup/descriptorgroup.h>
#include <dunya/gpu/device/device.h>
#include <dunya/renderer/materialrecord/materialrecord.h>
#include <dunya/gpu/sampler/sampler.h>
#include <dunya/gpu/texture/texture.h>

#include <cstdint>
#include <span>

namespace dunya::renderer {

class ResourceTable {
public:
  ResourceTable(
    const dunya::gpu::Device& device,
    std::span<const dunya::gpu::Texture> textures,
    std::span<const dunya::gpu::Sampler> samplers,
    std::span<const dunya::renderer::MaterialRecord> materials
  );

  ResourceTable(const ResourceTable&) = delete;
  ResourceTable& operator=(const ResourceTable&) = delete;
  ResourceTable(ResourceTable&&) = delete;
  ResourceTable& operator=(ResourceTable&&) = delete;

  ~ResourceTable() = default;

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

private:
  dunya::gpu::DescriptorGroup m_group;
};

}  // namespace dunya::renderer
