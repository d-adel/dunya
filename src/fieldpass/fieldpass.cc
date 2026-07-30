#include "fieldpass.ih"

FieldPass::FieldPass(
  const Device& device,
  const std::vector<Primitive>& primitives
)
    : m_group(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {{0,
          MAX_PRIMITIVES * sizeof(Primitive),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          false},
         {1, sizeof(FieldFrame), VK_SHADER_STAGE_FRAGMENT_BIT, true}}
      ),
      m_primitiveCount(static_cast<uint32_t>(primitives.size())) {
  if (primitives.size() > MAX_PRIMITIVES) {
    throw std::runtime_error("More primitives than the field buffer holds");
  }

  m_group.write(0, 0, primitives.data(), primitives.size() * sizeof(Primitive));
}

void FieldPass::update(uint32_t frame, const FieldFrame& frameData) {
  m_group.write(1, frame, &frameData, sizeof(frameData));
}

const VkDescriptorSetLayout& FieldPass::setLayout() const noexcept {
  return m_group.setLayout();
}

const VkDescriptorSet& FieldPass::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

uint32_t FieldPass::primitiveCount() const noexcept {
  return m_primitiveCount;
}
