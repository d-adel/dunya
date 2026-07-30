#pragma once

#include "device/device.h"
#include "buffer/buffer.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

class DescriptorGroup {
public:
  struct BufferBinding {
    uint32_t binding = 0;
    VkDeviceSize size = 0;
    VkShaderStageFlags stages = 0;
    bool perFrame = true;
  };

  struct ImageBinding {
    uint32_t binding = 0;
    VkShaderStageFlags stages = 0;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
  };

  DescriptorGroup(
    const Device& device,
    uint32_t frameCount,
    std::vector<BufferBinding> buffers,
    std::vector<ImageBinding> images = {}
  );

  DescriptorGroup(const DescriptorGroup&) = delete;
  DescriptorGroup& operator=(const DescriptorGroup&) = delete;
  DescriptorGroup(DescriptorGroup&&) = delete;
  DescriptorGroup& operator=(DescriptorGroup&&) = delete;

  ~DescriptorGroup();

  const VkDescriptorSetLayout& setLayout() const noexcept;
  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;

  void write(uint32_t binding, uint32_t frame, const void* data, size_t size);

private:
  struct Slot {
    uint32_t binding = 0;
    bool perFrame = true;
    VkDeviceSize size = 0;
    std::vector<Buffer> buffers;
    std::vector<void*> mapped;
  };

  void createSetLayout(
    const std::vector<BufferBinding>& buffers,
    const std::vector<ImageBinding>& images
  );
  void createBuffers(
    const Device& device,
    const std::vector<BufferBinding>& buffers
  );
  void createPool(size_t bufferCount, size_t imageCount);
  void createSets(const std::vector<ImageBinding>& images);

  VkDevice m_device = VK_NULL_HANDLE;
  uint32_t m_frameCount = 0;

  VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_pool = VK_NULL_HANDLE;

  std::vector<VkDescriptorSet> m_sets;
  std::vector<Slot> m_slots;
};
