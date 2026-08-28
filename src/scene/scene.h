#pragma once

#include <dunya/field/field.h>
#include <dunya/renderer/materialrecord/materialrecord.h>
#include <dunya/renderer/meshbuffers/meshbuffers.h>
#include <dunya/renderer/meshrecord/meshrecord.h>
#include <dunya/gpu/sampler/sampler.h>
#include <dunya/gpu/texture/texture.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/world/world.h>

#include <glm/glm.hpp>
#include <vector>

class Scene {
public:
  explicit Scene(const dunya::gpu::Context& context);
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  void augmentFrameContext(dunya::renderer::Frame& frameContext);

  const dunya::objectmodel::World& world() const noexcept;
  dunya::objectmodel::World& world() noexcept;

  const std::vector<dunya::renderer::MaterialRecord>&
  materials() const noexcept;
  const std::vector<dunya::gpu::Texture>& textures() const noexcept;
  const std::vector<dunya::gpu::Sampler>& samplers() const noexcept;

private:
  static std::vector<dunya::renderer::MaterialRecord> createMaterials();
  static std::vector<dunya::gpu::Texture> createTextures(
    const dunya::gpu::Device& device
  );
  static std::vector<dunya::gpu::Sampler> createSamplers(
    const dunya::gpu::Device& device
  );

  void addInitialPrimitives(dunya::objectmodel::Entity entity);

  std::vector<dunya::renderer::MaterialRecord> m_materials;
  std::vector<dunya::gpu::Sampler> m_samplers;
  std::vector<dunya::gpu::Texture> m_textures;
  std::vector<dunya::renderer::MeshBuffers> m_meshes;

  // Rebuilt every frame; a member so the span handed to Frame stays alive.
  std::vector<dunya::renderer::MeshRecord> m_meshRecords;

  dunya::objectmodel::World m_world;
};
