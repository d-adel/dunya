#include "fieldpass.ih"

using dunya::field::Primitive;

FieldPass::FieldPass(
  const Device& device,
  std::span<const dunya::field::Primitive> primitives
)
    : m_group(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {{0,
          MAX_PRIMITIVES * sizeof(Primitive),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          DescriptorGroup::BufferUpdate::PerFrameMutable,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
         {1,
          sizeof(FieldFrame),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          DescriptorGroup::BufferUpdate::PerFrame}}
      ) {
  if (primitives.size() > MAX_PRIMITIVES) {
    throw std::runtime_error("More primitives than the field buffer holds");
  }

  uploadPrimitives(primitives);
}

void FieldPass::uploadPrimitives(
  std::span<const dunya::field::Primitive> primitives
) {
  if (primitives.size() > MAX_PRIMITIVES) {
    throw std::runtime_error("More primitives than the field buffer holds");
  }

  m_group.write(0, 0, primitives.data(), primitives.size_bytes());
}

void FieldPass::update(uint32_t frame, const FieldFrame& frameData) {
  m_group.flush(frame);

  m_group.write(1, frame, &frameData, sizeof(frameData));
}

const VkDescriptorSetLayout& FieldPass::setLayout() const noexcept {
  return m_group.setLayout();
}

const VkDescriptorSet& FieldPass::descriptorSet(uint32_t frame) const noexcept {
  return m_group.descriptorSet(frame);
}
