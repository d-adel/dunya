#pragma once

#include "gpu/device/device.h"
#include "gpu/buffer/buffer.h"

#include <vulkan/vulkan.h>

#include <utility>

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
  void copyFrom(
    const Device& device,
    Buffer& buffer,
    uint32_t width,
    uint32_t height,
    uint32_t depth = 1
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
