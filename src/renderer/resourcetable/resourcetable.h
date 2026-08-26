#pragma once

#include "gpu/descriptorgroup/descriptorgroup.h"
#include "gpu/device/device.h"
#include "objectmodel/material/material.h"
#include "gpu/sampler/sampler.h"
#include "gpu/texture/texture.h"

#include <cstdint>
#include <span>

class ResourceTable {
public:
  ResourceTable(
    const Device& device,
    std::span<const Texture> textures,
    std::span<const Sampler> samplers,
    std::span<const Material> materials
  );

  ResourceTable(const ResourceTable&) = delete;
  ResourceTable& operator=(const ResourceTable&) = delete;
  ResourceTable(ResourceTable&&) = delete;
  ResourceTable& operator=(ResourceTable&&) = delete;

  ~ResourceTable() = default;

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

private:
  DescriptorGroup m_group;
};
