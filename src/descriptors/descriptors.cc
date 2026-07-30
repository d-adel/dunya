#include "descriptors.ih"

Descriptors::Descriptors(const Device& device, const Texture& texture)
    : m_group(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {{0, sizeof(UniformBufferObject), VK_SHADER_STAGE_VERTEX_BIT, true}},
        {{1,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          texture.image().imageView(),
          texture.sampler()}}
      ) {}

void Descriptors::update(
  uint32_t currentFrame,
  const UniformBufferObject& ubo
) {
  m_group.write(0, currentFrame, &ubo, sizeof(ubo));
}

const VkDescriptorSet& Descriptors::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& Descriptors::setLayout() const noexcept {
  return m_group.setLayout();
}
