#pragma once

#include "texture/texture.h"
#include "mesh/mesh.h"
#include "descriptors/descriptors.h"
#include "frame/frame.h"

#include <glm/glm.hpp>

class Scene {
public:
  Scene(const Context& context);
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  const Descriptors& descriptors() const noexcept;
  Descriptors& descriptors();

  void augmentFrameContext(Frame& frameContext) const;

private:
  Texture m_texture;
  Descriptors m_descriptors;

  std::vector<Mesh> m_meshes;
  std::vector<DrawItem> m_drawItems;
};
