#pragma once

#include <dunya/field/field.h>
#include <dunya/objectmodel/material/material.h>
#include <dunya/renderer/mesh/mesh.h>
#include <dunya/gpu/sampler/sampler.h>
#include <dunya/gpu/texture/texture.h>
#include <dunya/renderer/frame/frame.h>
#include <dunya/objectmodel/fieldobject/fieldobject.h>
#include <dunya/objectmodel/objectregistry/objectregistry.h>
#include <dunya/objectmodel/world/world.h>

#include <glm/glm.hpp>
#include <vector>

class Scene {
public:
  Scene(const dunya::gpu::Context& context);
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  void augmentFrameContext(dunya::renderer::Frame& frameContext);

  const dunya::objectmodel::World& world() const noexcept;
  dunya::objectmodel::World& world() noexcept;

  const std::vector<dunya::objectmodel::Material>& materials() const noexcept;
  const std::vector<dunya::gpu::Texture>& textures() const noexcept;
  const std::vector<dunya::gpu::Sampler>& samplers() const noexcept;

private:
  static std::vector<dunya::objectmodel::Material> createMaterials();
  static std::vector<dunya::gpu::Texture> createTextures(
    const dunya::gpu::Device& device
  );
  static std::vector<dunya::gpu::Sampler> createSamplers(
    const dunya::gpu::Device& device
  );

  void addInitialPrimitives(dunya::core::ObjectId objectId);

  std::vector<dunya::objectmodel::Material> m_materials;
  std::vector<dunya::gpu::Sampler> m_samplers;
  std::vector<dunya::gpu::Texture> m_textures;
  std::vector<dunya::renderer::Mesh> m_meshes;

  dunya::objectmodel::World m_world;
};
