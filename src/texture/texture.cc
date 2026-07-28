#include "texture.ih"

Texture::Texture(const Device& device, const std::string& filename)
    : m_device(device.vkDevice()) {
  createTextureImage(device, filename);
  createTextureSampler(device.physicalDevice());
}

Texture::~Texture() {
  destroy();
}

void Texture::destroy() noexcept {
  if (m_device == VK_NULL_HANDLE) {
    return;
  }

  vkDestroySampler(m_device, m_textureSampler, nullptr);
}

VkSampler Texture::textureSampler() const noexcept {
  return m_textureSampler;
}

const Image& Texture::image() const noexcept {
  return m_textureImage;
}

const VkSampler& Texture::sampler() const noexcept {
  return m_textureSampler;
}

void Texture::createTextureImage(
  const Device& device,
  const std::string& filename
) {
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load(
    filename.c_str(),
    &texWidth,
    &texHeight,
    &texChannels,
    STBI_rgb_alpha
  );
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("Failed to load texture image");
  }

  Buffer stagingBuffer(
    device,
    imageSize,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  void* data;
  vkMapMemory(m_device, stagingBuffer.memory(), 0, imageSize, 0, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(m_device, stagingBuffer.memory());

  stbi_image_free(pixels);

  uint32_t w = static_cast<uint32_t>(texWidth);
  uint32_t h = static_cast<uint32_t>(texHeight);
  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
  VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkImageUsageFlags usage =
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

  m_textureImage =
    Image(device, w, h, format, tiling, usage, properties, aspect);

  m_textureImage.transition(
    device,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  );

  m_textureImage.copyFrom(device, stagingBuffer, w, h);
  m_textureImage.transition(
    device,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  );
}

void Texture::createTextureSampler(VkPhysicalDevice physicalDevice) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);

  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;

  if (
    vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler)
    != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to create texture sampler");
  }
}
