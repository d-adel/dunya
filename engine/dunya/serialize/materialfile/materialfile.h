#pragma once

#include <dunya/core/asset/asset.h>
#include <dunya/core/config/config.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace dunya::serialize {

inline constexpr uint32_t MATERIAL_VERSION = 1u;

struct StoredTextureSlot {
  dunya::core::AssetId texture = dunya::core::INVALID_ASSET;
  uint32_t sampler = dunya::core::SAMPLER_LINEAR_REPEAT;
};

struct StoredMaterial {
  uint32_t version = MATERIAL_VERSION;

  glm::vec4 baseColor{1.0f};
  glm::vec4 emissive{0.0f, 0.0f, 0.0f, 1.0f};

  float metallic = 0.0f;
  float roughness = 1.0f;
  float normalScale = 1.0f;
  float occlusionStrength = 1.0f;
  float alphaCutoff = 0.5f;

  uint32_t flags = 0u;

  StoredTextureSlot baseColorTexture;
  StoredTextureSlot metallicRoughnessTexture;
  StoredTextureSlot normalTexture;
  StoredTextureSlot occlusionTexture;
  StoredTextureSlot emissiveTexture;
};

[[nodiscard]] std::string writeMaterial(const StoredMaterial& stored);

[[nodiscard]] bool readMaterial(std::string_view text, StoredMaterial& stored);

}
