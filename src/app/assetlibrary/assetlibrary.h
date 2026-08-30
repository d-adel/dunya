#pragma once

#include <dunya/core/asset/assetdatabase.h>
#include <dunya/serialize/materialfile/materialfile.h>
#include <dunya/gpu/context/context.h>
#include <dunya/gpu/sampler/sampler.h>
#include <dunya/gpu/texture/texture.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/renderer/materialrecord/materialrecord.h>
#include <dunya/renderer/meshbuffers/meshbuffers.h>
#include <dunya/renderer/meshrecord/meshrecord.h>

#include <filesystem>
#include <vector>

class AssetLibrary {
public:
  AssetLibrary(
    const dunya::gpu::Context& context,
    const std::filesystem::path& projectRoot
  );

  AssetLibrary(const AssetLibrary&) = delete;
  AssetLibrary& operator=(const AssetLibrary&) = delete;
  AssetLibrary(AssetLibrary&&) = delete;
  AssetLibrary& operator=(AssetLibrary&&) = delete;

  void augmentFrameContext(
    dunya::renderer::Frame& frameContext,
    const dunya::objectmodel::World& world
  );

  [[nodiscard]] const std::vector<dunya::renderer::MaterialRecord>&
  materials() const noexcept;

  [[nodiscard]] const std::vector<dunya::gpu::Texture>&
  textures() const noexcept;

  [[nodiscard]] const std::vector<dunya::gpu::Sampler>&
  samplers() const noexcept;

  [[nodiscard]] const dunya::core::AssetDatabase& assets() const noexcept;

  [[nodiscard]] uint32_t materialIndex(dunya::core::AssetId id) const;
  [[nodiscard]] uint32_t textureIndex(dunya::core::AssetId id) const;
  [[nodiscard]] uint32_t meshIndex(dunya::core::AssetId id) const;

private:
  void loadProject(
    const dunya::gpu::Device& device,
    const std::filesystem::path& projectRoot
  );

  uint32_t loadMesh(
    const dunya::gpu::Device& device,
    dunya::core::AssetId id,
    const char* path
  );

  uint32_t loadTexture(
    const dunya::gpu::Device& device,
    dunya::core::AssetId id,
    const char* path
  );

  uint32_t addMaterial(
    dunya::core::AssetId id,
    const dunya::serialize::StoredMaterial& stored
  );

  static std::vector<dunya::gpu::Texture> createTextures(
    const dunya::gpu::Device& device
  );

  static std::vector<dunya::gpu::Sampler> createSamplers(
    const dunya::gpu::Device& device
  );

  dunya::core::AssetDatabase m_assets;

  std::vector<dunya::renderer::MaterialRecord> m_materials;
  std::vector<dunya::gpu::Sampler> m_samplers;
  std::vector<dunya::gpu::Texture> m_textures;
  std::vector<dunya::renderer::MeshBuffers> m_meshes;

  std::vector<dunya::renderer::MeshRecord> m_meshRecords;
};
