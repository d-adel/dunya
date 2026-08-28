#pragma once

#include <dunya/gpu/device/device.h>
#include <dunya/gpu/buffer/buffer.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace dunya::gpu {

class DescriptorGroup {
public:
  enum class BufferUpdate {
    // Written once at construction and never again: one copy is enough.
    Static,
    // Written every frame with that frame's own value.
    PerFrame
  };

  struct BufferBinding {
    uint32_t binding = 0;
    VkDeviceSize size = 0;
    VkShaderStageFlags stages = 0;
    BufferUpdate update = BufferUpdate::PerFrame;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  };

  struct SampledImageBinding {
    uint32_t binding = 0;
    VkShaderStageFlags stages = 0;
    std::vector<VkImageView> elements{};
    uint32_t capacity = 0;
  };

  // Same shape as a sampled image, but written by a shader rather than read,
  // which means the descriptor names VK_IMAGE_LAYOUT_GENERAL and the image has
  // to actually be in that layout when the shader runs.
  struct StorageImageBinding {
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

  // A storage buffer this group describes but does not own: one the GPU both
  // writes and reads wants to be device-local, so its owner allocates it.
  struct DeviceBufferBinding {
    uint32_t binding = 0;
    VkShaderStageFlags stages = 0;
  };

  DescriptorGroup(
    const Device& device,
    uint32_t frameCount,
    std::vector<BufferBinding> buffers,
    std::vector<SampledImageBinding> sampledImages = {},
    std::vector<SamplerBinding> samplers = {},
    std::vector<StorageImageBinding> storageImages = {},
    std::vector<DeviceBufferBinding> deviceBuffers = {}
  );

  DescriptorGroup(const DescriptorGroup&) = delete;
  DescriptorGroup& operator=(const DescriptorGroup&) = delete;
  DescriptorGroup(DescriptorGroup&&) = delete;
  DescriptorGroup& operator=(DescriptorGroup&&) = delete;

  ~DescriptorGroup();

  const VkDescriptorSetLayout& setLayout() const noexcept;
  const VkDescriptorSet& descriptorSet(uint32_t frame) const noexcept;

  void write(uint32_t binding, uint32_t frame, const void* data, size_t size);
  void writeImage(uint32_t binding, uint32_t index, VkImageView imageView);

  // Same, for a storage-image binding: GENERAL layout instead of read-only.
  void writeStorageImage(
    uint32_t binding,
    uint32_t index,
    VkImageView imageView
  );

  // Points a device-buffer binding at the buffer its owner allocated. The
  // group never writes through it, so there is no mapping and no frame index.
  void writeBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize size);

private:
  struct Slot {
    uint32_t binding = 0;
    BufferUpdate update = BufferUpdate::PerFrame;
    VkDeviceSize size = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    std::vector<Buffer> buffers;
    std::vector<void*> mapped;
  };

  struct ImageSlot {
    uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    uint32_t capacity = 0;
  };

  void createSetLayout(
    const std::vector<BufferBinding>& buffers,
    const std::vector<SampledImageBinding>& sampledImages,
    const std::vector<SamplerBinding>& samplers,
    const std::vector<StorageImageBinding>& storageImages,
    const std::vector<DeviceBufferBinding>& deviceBuffers
  );

  void createBuffers(
    const Device& device,
    const std::vector<BufferBinding>& buffers
  );
  void createPool(
    const std::vector<SampledImageBinding>& sampledImages,
    const std::vector<SamplerBinding>& samplers,
    const std::vector<StorageImageBinding>& storageImages,
    const std::vector<DeviceBufferBinding>& deviceBuffers
  );
  void createSets(
    const std::vector<SampledImageBinding>& sampledImages,
    const std::vector<SamplerBinding>& samplers,
    const std::vector<StorageImageBinding>& storageImages
  );

  VkDevice m_device = VK_NULL_HANDLE;
  uint32_t m_frameCount = 0;

  VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_pool = VK_NULL_HANDLE;

  // Decided once by createSetLayout from the flags it used, because a layout
  // carrying the bit needs a pool that carries it too. Re-deriving it in
  // createPool would be a second source of truth.
  bool m_updateAfterBind = false;

  std::vector<VkDescriptorSet> m_sets;
  std::vector<Slot> m_slots;
  std::vector<ImageSlot> m_imageSlots;
  std::vector<uint32_t> m_deviceBufferSlots;
};

}  // namespace dunya::gpu
