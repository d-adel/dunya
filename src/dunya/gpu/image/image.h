#pragma once

#include <dunya/gpu/device/device.h>
#include <dunya/gpu/buffer/buffer.h>

#include <vulkan/vulkan.h>

#include <utility>

namespace dunya::gpu {

class Image {
public:
  Image() = default;
  // depth of 1 makes a 2D image, anything more a 3D one. The distinction
  // reaches the image type, the view type and the copy extent together, so it
  // is taken once rather than inferred in three places.
  Image(
    const Device& device,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImageAspectFlags aspect
  );

  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  Image(Image&& other) noexcept;
  Image& operator=(Image&& other) noexcept;

  ~Image();

  // Static
  static VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags,
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D
  );

  // Getters
  VkImage image() const noexcept;
  VkImageView imageView() const noexcept;
  VkDeviceMemory memory() const noexcept;

  // Helpers
  void copyTo(const Device& device, VkBuffer dst, VkDeviceSize size) const;

  void transition(
    const Device& device,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
  );
  // The offset makes this a sub-region copy: a dent rewrites a few dozen
  // voxels of a 128-cubed volume, and staging the whole grid for that is
  // 10 MiB of traffic for 30 KB of change.
  void copyFrom(
    const Device& device,
    Buffer& buffer,
    uint32_t width,
    uint32_t height,
    uint32_t depth = 1,
    VkOffset3D offset = {0, 0, 0}
  );

  // The same two, recorded rather than submitted. Load-time callers want the
  // pair above, which submit and wait; a caller inside a frame wants these,
  // so a dozen copies become one submission and no stall. See Uploader.
  void recordTransition(
    VkCommandBuffer commandBuffer,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
  );

  void recordCopyFrom(
    VkCommandBuffer commandBuffer,
    const Buffer& buffer,
    uint32_t width,
    uint32_t height,
    uint32_t depth = 1,
    VkOffset3D offset = {0, 0, 0}
  );

private:
  void destroy() noexcept;
  void createImage(
    const Device& device,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties
  );

  VkImage m_image = VK_NULL_HANDLE;
  VkImageView m_imageView = VK_NULL_HANDLE;
  VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;

  VkDevice m_device = VK_NULL_HANDLE;
};

}  // namespace dunya::gpu
