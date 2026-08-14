#include "frameglobals.ih"

FrameGlobals::FrameGlobals(const Device& device)
    : m_group(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {{0,
          sizeof(CameraUniform),
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          true}}
      ) {}

void FrameGlobals::update(uint32_t frame, const CameraUniform& camera) {
  m_group.write(0, frame, &camera, sizeof(camera));
}

const VkDescriptorSet& FrameGlobals::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& FrameGlobals::setLayout() const noexcept {
  return m_group.setLayout();
}
