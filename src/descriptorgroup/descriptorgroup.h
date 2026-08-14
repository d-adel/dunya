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

  struct SampledImageBinding {
    uint32_t binding = 0;
    VkShaderStageFlags stages = 0;
    std::vector<VkImageView> elements{};
    uint32_t capacity = 0;
  };

  struct SamplerBinding {
    uint32_t binding = 0;
    VkShaderStageFlags stages = 0;
    std::vector<VkSampler> elements{};
    uint32_t capacity = 0;
  };

  DescriptorGroup(
    const Device& device,
    uint32_t frameCount,
    std::vector<BufferBinding> buffers,
    std::vector<SampledImageBinding> sampledImages = {},
    std::vector<SamplerBinding> samplers = {}
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
    const std::vector<SampledImageBinding>& sampledImages,
    const std::vector<SamplerBinding>& samplers
  );
  void createBuffers(
    const Device& device,
    const std::vector<BufferBinding>& buffers
  );
  void createPool(
    size_t bufferCount,
    const std::vector<SampledImageBinding>& sampledImages,
    const std::vector<SamplerBinding>& samplers
  );
  void createSets(
    const std::vector<SampledImageBinding>& sampledImages,
    const std::vector<SamplerBinding>& samplers
  );

  VkDevice m_device = VK_NULL_HANDLE;
  uint32_t m_frameCount = 0;

  VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_pool = VK_NULL_HANDLE;

  std::vector<VkDescriptorSet> m_sets;
  std::vector<Slot> m_slots;
};
