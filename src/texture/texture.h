#pragma once

#include "device/device.h"
#include "image/image.h"

#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <utility>
#include <string>

class Texture {
public:
  Texture(const Device& device, const std::string& texturePath);

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  ~Texture();

  VkSampler textureSampler() const noexcept;
  const Image& image() const noexcept;
  const VkSampler& sampler() const noexcept;

private:
  void destroy() noexcept;

  void createTextureImage(const Device& device, const std::string& texturePath);
  void createTextureSampler(VkPhysicalDevice physicalDevice);

  VkDevice m_device = VK_NULL_HANDLE;

  Image m_textureImage;
  VkSampler m_textureSampler = VK_NULL_HANDLE;
};
