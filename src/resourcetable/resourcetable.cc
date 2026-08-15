#include "resourcetable.ih"

static std::vector<VkImageView> imageViews(std::span<const Texture> textures) {
  std::vector<VkImageView> views;
  views.reserve(textures.size());

  for (const Texture& texture : textures) {
    views.push_back(texture.image().imageView());
  }

  return views;
}

static std::vector<VkSampler> samplerHandles(
  std::span<const Sampler> samplers
) {
  std::vector<VkSampler> handles;
  handles.reserve(samplers.size());

  for (const Sampler& sampler : samplers) {
    handles.push_back(sampler.handle());
  }

  return handles;
}

ResourceTable::ResourceTable(
  const Device& device,
  std::span<const Texture> textures,
  std::span<const Sampler> samplers,
  std::span<const Material> materials
)
    : m_group(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {{0,
          MAX_MATERIALS * sizeof(Material),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          false}},
        {{1, VK_SHADER_STAGE_FRAGMENT_BIT, imageViews(textures), MAX_TEXTURES}},
        {{2,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          samplerHandles(samplers),
          MAX_SAMPLERS}}
      ) {
  if (materials.size() > MAX_MATERIALS) {
    throw std::runtime_error("More materials than the material table holds");
  }

  // PARTIALLY_BOUND makes an unwritten array slot undefined rather than an
  // error, so an out-of-range index here would sample garbage silently.
  for (const Material& material : materials) {
    const std::array<uint32_t, 5> images{
      material.baseColorTexture,
      material.metallicRoughnessTexture,
      material.normalTexture,
      material.occlusionTexture,
      material.emissiveTexture
    };

    for (uint32_t image : images) {
      if (image >= textures.size()) {
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
      if (index >= samplers.size()) {
        throw std::runtime_error("Material names a sampler slot with none");
      }
    }
  }

  m_group.write(0, 0, materials.data(), materials.size_bytes());
}

const VkDescriptorSet& ResourceTable::descriptorSet(
  uint32_t frame
) const noexcept {
  return m_group.descriptorSet(frame);
}

const VkDescriptorSetLayout& ResourceTable::setLayout() const noexcept {
  return m_group.setLayout();
}
