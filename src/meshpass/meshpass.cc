#include "meshpass.ih"

MeshPass::MeshPass(const Device& device)
    : m_texture(device, "textures/viking_room.png"),
      m_group(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {{0, sizeof(UniformBufferObject), VK_SHADER_STAGE_VERTEX_BIT, true}},
        {{1,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          m_texture.image().imageView(),
          m_texture.sampler()}}
      ) {}

void MeshPass::update(uint32_t currentFrame, const UniformBufferObject& ubo) {
  m_group.write(0, currentFrame, &ubo, sizeof(ubo));
}

const VkDescriptorSet& MeshPass::descriptorSet(uint32_t frame) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& MeshPass::setLayout() const noexcept {
  return m_group.setLayout();
}
