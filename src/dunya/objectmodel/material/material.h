#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace dunya::objectmodel {

constexpr uint32_t MATERIAL_FLAG_DOUBLE_SIDED = 1u << 0;
constexpr uint32_t MATERIAL_FLAG_ALPHA_MASK = 1u << 1;
constexpr uint32_t MATERIAL_FLAG_ALPHA_BLEND = 1u << 2;

struct Material {
  glm::vec4 baseColor;
  glm::vec4 emissive;

  float metallic;
  float roughness;
  float normalScale;
  float occlusionStrength;

  float alphaCutoff;
  uint32_t flags;
  uint32_t baseColorTexture;
  uint32_t baseColorSampler;

  uint32_t metallicRoughnessTexture;
  uint32_t metallicRoughnessSampler;
  uint32_t normalTexture;
  uint32_t normalSampler;

  uint32_t occlusionTexture;
  uint32_t occlusionSampler;
  uint32_t emissiveTexture;
  uint32_t emissiveSampler;
};

static_assert(
  sizeof(Material) == 96,
  "Material must keep the std140 layout the shaders index by"
);

}  // namespace dunya::objectmodel
