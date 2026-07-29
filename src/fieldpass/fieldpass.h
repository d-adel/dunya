#pragma once

#include "field/field.h"
#include "device/device.h"
#include "buffer/buffer.h"
#include <vulkan/vulkan.h>
#include <vector>

constexpr int MAX_PRIMITIVES = 128;

class FieldPass {
public:
  FieldPass(const Device&, const std::vector<Primitive>&);
  ~FieldPass();

  FieldPass(const FieldPass&) = delete;
  FieldPass& operator=(const FieldPass&) = delete;
  FieldPass(FieldPass&&) = delete;
  FieldPass& operator=(FieldPass&&) = delete;

  const VkDescriptorSetLayout& setLayout() const noexcept;
  const VkDescriptorSet& descriptorSet() const noexcept;
  const std::vector<Primitive>& primitives() const noexcept;

private:
  void createSetLayout();
  void createUniformBuffer(const Device& device);
  void createDescriptorPool();
  void createDescriptorSet();

  VkDevice m_device = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

  VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
  Buffer m_uniformBuffer;
  void* m_uniformBufferMapped = nullptr;

  std::vector<Primitive> m_primitives;
};
