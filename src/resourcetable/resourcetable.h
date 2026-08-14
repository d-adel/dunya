#pragma once

#include "descriptorgroup/descriptorgroup.h"
#include "device/device.h"
#include "material/material.h"
#include "sampler/sampler.h"
#include "texture/texture.h"

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
