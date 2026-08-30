#pragma once

#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>
#include <dunya/renderer/materialrecord/materialrecord.h>
#include <dunya/renderer/meshbuffers/meshbuffers.h>
#include <dunya/renderer/meshrecord/meshrecord.h>
#include <dunya/gpu/sampler/sampler.h>
#include <dunya/gpu/texture/texture.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/core/asset/asset.h>
#include <dunya/objectmodel/camera/camera.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/world/world.h>

#include <glm/glm.hpp>
#include <vector>

class Scene {
public:
  Scene(
    const dunya::gpu::Context& context,
    dunya::objectmodel::World& world,
    uint32_t wallColumns,
    uint32_t wallRows,
    uint32_t wallDepth
  );
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  void augmentFrameContext(
    dunya::renderer::Frame& frameContext,
    const dunya::objectmodel::World& world
  );

  const std::vector<dunya::renderer::MaterialRecord>&
  materials() const noexcept;
  const std::vector<dunya::gpu::Texture>& textures() const noexcept;
  const std::vector<dunya::gpu::Sampler>& samplers() const noexcept;

  struct Projectile {
    dunya::objectmodel::SdfGrid grid;
    dunya::field::Primitive shape;
    float speed = 0.0f;

    float height = 0.0f;
    glm::vec3 aimAt{0.0f};

    float mass = 100.0f;
  };

  [[nodiscard]]
  Projectile projectile() const;

  [[nodiscard]]
  const dunya::field::SampledField& projectileField() const noexcept;

  void frame(dunya::objectmodel::Camera& camera) const;

  glm::vec3 wallPoint(float u, float v) const;

  [[nodiscard]]
  dunya::objectmodel::Entity deformable() const noexcept;

  [[nodiscard]] uint32_t materialIndex(dunya::core::AssetId id) const;
  [[nodiscard]] uint32_t meshIndex(dunya::core::AssetId id) const;

  [[nodiscard]] const dunya::core::AssetRegistry&
  materialAssets() const noexcept;

  [[nodiscard]] const dunya::core::AssetRegistry& meshAssets() const noexcept;

private:
  std::vector<dunya::renderer::MaterialRecord> createMaterials();
  static std::vector<dunya::gpu::Texture> createTextures(
    const dunya::gpu::Device& device
  );
  static std::vector<dunya::gpu::Sampler> createSamplers(
    const dunya::gpu::Device& device
  );

  void addPrimitive(
    dunya::objectmodel::Entity entity,
    const dunya::field::Primitive& primitive,
    const char* what
  );

  dunya::core::AssetRegistry m_materialAssets;
  dunya::core::AssetRegistry m_meshAssets;

  std::vector<dunya::renderer::MaterialRecord> m_materials;
  std::vector<dunya::gpu::Sampler> m_samplers;
  std::vector<dunya::gpu::Texture> m_textures;
  std::vector<dunya::renderer::MeshBuffers> m_meshes;

  std::vector<dunya::renderer::MeshRecord> m_meshRecords;

  dunya::objectmodel::Entity m_deformable = dunya::objectmodel::INVALID_ENTITY;

  glm::vec3 m_wallMinimum{0.0f};
  glm::vec3 m_wallMaximum{0.0f};

  dunya::objectmodel::World& m_world;

  dunya::field::SampledField m_projectileField;
};
