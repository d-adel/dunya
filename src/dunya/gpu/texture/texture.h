#pragma once

#include <dunya/gpu/device/device.h>
#include <dunya/gpu/image/image.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace dunya::gpu {

class Texture {
public:
  Texture() = default;
  Texture(const Device& device, const std::string& texturePath);

  // Four bytes per texel, which is what every 2D format here uses.
  Texture(
    const Device& device,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    const void* pixels
  );

  // The general form: any format, so the caller states the byte count. Extra
  // usage is for images a shader writes, which need STORAGE_BIT at creation.
  Texture(
    const Device& device,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    VkFormat format,
    const void* data,
    VkDeviceSize sizeBytes,
    VkImageUsageFlags extraUsage = 0
  );

  const Image& image() const noexcept;

private:
  void createTextureImage(
    const Device& device,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    VkFormat format,
    const void* data,
    VkDeviceSize sizeBytes,
    VkImageUsageFlags extraUsage
  );

  Image m_textureImage;
};

}  // namespace dunya::gpu
