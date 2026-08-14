#pragma once

#include "device/device.h"
#include "image/image.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

class Texture {
public:
  Texture(const Device& device, const std::string& texturePath);
  Texture(
    const Device& device,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    const void* pixels
  );

  const Image& image() const noexcept;

private:
  void createTextureImage(
    const Device& device,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    const void* pixels
  );

  Image m_textureImage;
};
