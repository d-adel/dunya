#include "frameglobals.ih"

namespace dunya::renderer {

namespace {

// std140 rounds a block up to sixteen bytes; the struct is 28, so the binding
// has to reserve 32 or the descriptor range is smaller than the block.
constexpr VkDeviceSize MARCH_PARAMS_BLOCK_BYTES =
  (sizeof(MarchParams) + 15u) / 16u * 16u;

}  // namespace

FrameGlobals::FrameGlobals(const dunya::gpu::Device& device)
    : m_group(
        device,
        dunya::core::MAX_FRAMES_IN_FLIGHT,
        {{0,
          sizeof(CameraUniform),
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame},
         // Set 0 because it changes at the same rate as the camera: this set is
         // the per-frame one, and a slider moves a value exactly as often.
         {1,
          MARCH_PARAMS_BLOCK_BYTES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame}}
      ) {}

void FrameGlobals::update(
  uint32_t frame,
  const CameraUniform& camera,
  const MarchParams& march
) {
  m_group.write(0, frame, &camera, sizeof(camera));
  m_group.write(1, frame, &march, sizeof(march));
}

const VkDescriptorSet& FrameGlobals::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& FrameGlobals::setLayout() const noexcept {
  return m_group.setLayout();
}

}  // namespace dunya::renderer
