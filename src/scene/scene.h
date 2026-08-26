#pragma once

#include "field/field.h"
#include "objectmodel/material/material.h"
#include "renderer/mesh/mesh.h"
#include "gpu/sampler/sampler.h"
#include "gpu/texture/texture.h"
#include "renderer/frame/frame.h"
#include "objectmodel/fieldobject/fieldobject.h"
#include "objectmodel/objectregistry/objectregistry.h"
#include "objectmodel/world/world.h"

#include <glm/glm.hpp>
#include <vector>

class Scene {
public:
  Scene(const Context& context);
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  void augmentFrameContext(Frame& frameContext);

  const World& world() const noexcept;
  World& world() noexcept;

  const std::vector<Material>& materials() const noexcept;
  const std::vector<Texture>& textures() const noexcept;
  const std::vector<Sampler>& samplers() const noexcept;

private:
  static std::vector<Material> createMaterials();
  static std::vector<Texture> createTextures(const Device& device);
  static std::vector<Sampler> createSamplers(const Device& device);

  void addInitialPrimitives(ObjectId objectId);

  std::vector<Material> m_materials;
  std::vector<Sampler> m_samplers;
  std::vector<Texture> m_textures;
  std::vector<Mesh> m_meshes;

  World m_world;
};
