#pragma once

#include "texture/texture.h"
#include "mesh/mesh.h"
#include "meshpass/meshpass.h"
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

  const MeshPass& meshPass() const noexcept;
  MeshPass& meshPass();

  void augmentFrameContext(Frame& frameContext) const;

private:
  Texture m_texture;
  MeshPass m_meshPass;

  std::vector<Mesh> m_meshes;
  std::vector<DrawItem> m_drawItems;
};
