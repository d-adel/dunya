#pragma once

#include "gpu/device/device.h"
#include "gpu/image/image.h"

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

  // The general form: a volume of any format, so the caller states the byte
  // count rather than having it inferred from a texel size that varies. Extra
  // usage is for images a shader will also write, which need STORAGE_BIT
  // declared at creation and cannot gain it later.
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
