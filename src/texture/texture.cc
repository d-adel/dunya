#include "texture.ih"

Texture::Texture(const Device& device, const std::string& texturePath) {
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load(
    texturePath.c_str(),
    &texWidth,
    &texHeight,
    &texChannels,
    STBI_rgb_alpha
  );

  if (!pixels) {
    throw std::runtime_error("Failed to load texture image");
  }

  createTextureImage(
    device,
    static_cast<uint32_t>(texWidth),
    static_cast<uint32_t>(texHeight),
    VK_FORMAT_R8G8B8A8_SRGB,
    pixels
  );

  stbi_image_free(pixels);
}

Texture::Texture(
  const Device& device,
  uint32_t width,
  uint32_t height,
  VkFormat format,
  const void* pixels
) {
  createTextureImage(device, width, height, format, pixels);
}

const Image& Texture::image() const noexcept {
  return m_textureImage;
}

void Texture::createTextureImage(
  const Device& device,
  uint32_t width,
  uint32_t height,
  VkFormat format,
  const void* pixels
) {
  VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

  Buffer stagingBuffer(
    device,
    imageSize,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );

  void* data;
  vkMapMemory(
    device.vkDevice(),
    stagingBuffer.memory(),
    0,
    imageSize,
    0,
    &data
  );
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(device.vkDevice(), stagingBuffer.memory());

  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
  VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkImageUsageFlags usage =
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

  m_textureImage =
    Image(device, width, height, format, tiling, usage, properties, aspect);

  m_textureImage.transition(
    device,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  );

  m_textureImage.copyFrom(device, stagingBuffer, width, height);
  m_textureImage.transition(
    device,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  );
}
