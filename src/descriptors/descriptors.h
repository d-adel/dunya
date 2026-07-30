#pragma once

#include "descriptorgroup/descriptorgroup.h"
#include "device/device.h"
#include "texture/texture.h"
#include "ubo/ubo.h"

#include <cstdint>

class Descriptors {
public:
  Descriptors(const Device& device, const Texture& texture);

  Descriptors(Descriptors const&) = delete;
  Descriptors& operator=(Descriptors const&) = delete;
  Descriptors(Descriptors&&) = delete;
  Descriptors& operator=(Descriptors&&) = delete;

  ~Descriptors() = default;

  void update(uint32_t currentFrame, const UniformBufferObject& ubo);

  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;
  const VkDescriptorSetLayout& setLayout() const noexcept;

private:
  DescriptorGroup m_group;
};
