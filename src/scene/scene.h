#pragma once

#include "field/field.h"
#include "material/material.h"
#include "mesh/mesh.h"
#include "sampler/sampler.h"
#include "texture/texture.h"
#include "frame/frame.h"

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

  void augmentFrameContext(Frame& frameContext) const;

  const std::vector<Primitive>& primitives() const noexcept;
  const std::vector<Material>& materials() const noexcept;
  const std::vector<Texture>& textures() const noexcept;
  const std::vector<Sampler>& samplers() const noexcept;

private:
  static std::vector<Primitive> createPrimitives();
  static std::vector<Material> createMaterials();
  static std::vector<Texture> createTextures(const Device& device);
  static std::vector<Sampler> createSamplers(const Device& device);

  std::vector<Primitive> m_primitives;
  std::vector<Material> m_materials;
  std::vector<Sampler> m_samplers;
  std::vector<Texture> m_textures;
  std::vector<Mesh> m_meshes;
  std::vector<DrawItem> m_drawItems;
};
