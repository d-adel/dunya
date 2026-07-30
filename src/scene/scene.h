#pragma once

#include "field/field.h"
#include "mesh/mesh.h"
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

private:
  static std::vector<Primitive> createPrimitives();

  std::vector<Primitive> m_primitives;
  std::vector<Mesh> m_meshes;
  std::vector<DrawItem> m_drawItems;
};
