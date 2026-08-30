#include "frameglobals.ih"

namespace dunya::renderer {

namespace {

constexpr VkDeviceSize MARCH_PARAMS_BLOCK_BYTES =
  (sizeof(MarchParams) + 15u) / 16u * 16u;

constexpr VkDeviceSize SCENE_COUNTS_BLOCK_BYTES =
  (sizeof(SceneCounts) + 15u) / 16u * 16u;

}  // namespace

FrameGlobals::FrameGlobals(const dunya::gpu::Device& device)
    : m_group(
        device,
        dunya::core::MAX_FRAMES_IN_FLIGHT,
        {{0,
          sizeof(CameraUniform),
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame},
         {1,
          MARCH_PARAMS_BLOCK_BYTES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame},
         {2,
          SCENE_COUNTS_BLOCK_BYTES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame},
         {3,
          sizeof(LightUniform),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::PerFrame}}
      ) {}

void FrameGlobals::update(
  uint32_t frame,
  const CameraUniform& camera,
  const MarchParams& march,
  const SceneCounts& counts,
  const LightUniform& light
) {
  m_group.write(0, frame, &camera, sizeof(camera));
  m_group.write(1, frame, &march, sizeof(march));
  m_group.write(2, frame, &counts, sizeof(counts));
  m_group.write(3, frame, &light, sizeof(light));
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
