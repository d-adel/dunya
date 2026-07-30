#pragma once

#include "descriptorgroup/descriptorgroup.h"
#include "device/device.h"
#include "texture/texture.h"
#include "ubo/ubo.h"

#include <cstdint>

class MeshPass {
public:
  MeshPass(const Device& device);

  MeshPass(MeshPass const&) = delete;
  MeshPass& operator=(MeshPass const&) = delete;
  MeshPass(MeshPass&&) = delete;
  MeshPass& operator=(MeshPass&&) = delete;

  ~MeshPass() = default;

  void update(uint32_t currentFrame, const UniformBufferObject& ubo);

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

private:
  Texture m_texture;
  DescriptorGroup m_group;
};
