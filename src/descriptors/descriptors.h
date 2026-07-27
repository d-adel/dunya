#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "device/device.h"
#include "buffer/buffer.h"
#include "texture/texture.h"
#include "ubo/ubo.h"

#include <vector>

class Descriptors {
public:
  Descriptors(const Device& device, const Texture& texture);
  Descriptors(Descriptors const&) = delete;
  Descriptors& operator=(Descriptors const&) = delete;

  ~Descriptors();

  void update(uint32_t currentFrame, const UniformBufferObject& ubo);

  const std::vector<VkDescriptorSet>& descriptorSets() const noexcept;
  const VkDescriptorSetLayout& descriptorSetLayout() const noexcept;

private:
  void createDescriptorSetLayout();
  void createUniformBuffers(const Device& device);
  void createDescriptorPool();
  void createDescriptorSets(const Texture& texture);

  VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

  std::vector<VkDescriptorSet> m_descriptorSets;
  std::vector<Buffer> m_uniformBuffers;
  std::vector<void*> m_uniformBuffersMapped;
};
