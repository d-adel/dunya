#include "resourcetable.ih"

namespace dunya::renderer {

static std::vector<VkImageView> imageViews(
  std::span<const dunya::gpu::Texture> textures
) {
  std::vector<VkImageView> views;
  views.reserve(textures.size());

  for (const dunya::gpu::Texture& texture : textures) {
    views.push_back(texture.image().imageView());
  }

  return views;
}

static std::vector<VkSampler> samplerHandles(
  std::span<const dunya::gpu::Sampler> samplers
) {
  std::vector<VkSampler> handles;
  handles.reserve(samplers.size());

  for (const dunya::gpu::Sampler& sampler : samplers) {
    handles.push_back(sampler.handle());
  }

  return handles;
}

ResourceTable::ResourceTable(
  const dunya::gpu::Device& device,
  std::span<const dunya::gpu::Texture> textures,
  std::span<const dunya::gpu::Sampler> samplers,
  std::span<const dunya::renderer::MaterialRecord> materials
)
    : m_group(
        device,
        dunya::core::MAX_FRAMES_IN_FLIGHT,
        {{MATERIALS,
          dunya::core::MAX_MATERIALS * sizeof(dunya::renderer::MaterialRecord),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          dunya::gpu::DescriptorGroup::BufferUpdate::Static}},
        {{TEXTURES,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          imageViews(textures),
          dunya::core::MAX_TEXTURES}},
        {{SAMPLERS,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          samplerHandles(samplers),
          dunya::core::MAX_SAMPLERS}}
      ),
      m_textureCount(static_cast<uint32_t>(textures.size())),
      m_samplerCount(static_cast<uint32_t>(samplers.size())) {
  refresh(materials);
}

void ResourceTable::refresh(
  std::span<const dunya::renderer::MaterialRecord> materials
) {
  if (materials.size() > dunya::core::MAX_MATERIALS) {
    throw std::runtime_error("More materials than the material table holds");
  }

  for (const dunya::renderer::MaterialRecord& material : materials) {
    const std::array<uint32_t, 5> images{
      material.baseColorTexture,
      material.metallicRoughnessTexture,
      material.normalTexture,
      material.occlusionTexture,
      material.emissiveTexture
    };

    for (uint32_t image : images) {
      if (image >= m_textureCount) {
        throw std::runtime_error("Material names a texture slot with no image");
      }
    }

    const std::array<uint32_t, 5> used{
      material.baseColorSampler,
      material.metallicRoughnessSampler,
      material.normalSampler,
      material.occlusionSampler,
      material.emissiveSampler
    };

    for (uint32_t index : used) {
      if (index >= m_samplerCount) {
        throw std::runtime_error("Material names a sampler slot with none");
      }
    }
  }

  m_group.write(MATERIALS, 0, materials.data(), materials.size_bytes());
}

const VkDescriptorSet& ResourceTable::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& ResourceTable::setLayout() const noexcept {
  return m_group.setLayout();
}

}
