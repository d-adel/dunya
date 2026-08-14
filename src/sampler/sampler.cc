#include "sampler.ih"

Sampler::Sampler(const Device& device) : m_device(device.vkDevice()) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(device.physicalDevice(), &properties);

  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

  if (
    vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS
  ) {
    throw std::runtime_error("Failed to create texture sampler");
  }
}

Sampler::Sampler(Sampler&& other) noexcept
    : m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE)),
      m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

Sampler& Sampler::operator=(Sampler&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  destroy();

  m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);

  m_device = std::exchange(other.m_device, VK_NULL_HANDLE);

  return *this;
}

Sampler::~Sampler() {
  destroy();
}

VkSampler Sampler::handle() const noexcept {
  return m_sampler;
}

void Sampler::destroy() noexcept {
  if (m_device == VK_NULL_HANDLE) {
    return;
  }

  vkDestroySampler(m_device, m_sampler, nullptr);

  m_sampler = VK_NULL_HANDLE;
}
