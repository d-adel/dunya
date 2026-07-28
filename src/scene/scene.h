#pragma once

#include "swapchain/swapchain.h"
#include "texture/texture.h"
#include "mesh/mesh.h"
#include "descriptors/descriptors.h"
#include "pipeline/pipeline.h"
#include "frame/frame.h"

#include <glm/glm.hpp>

class Scene {
public:
  Scene(const Context& context, const SwapChain& swapChain);
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  const Descriptors& descriptors() const noexcept;
  Descriptors& descriptors();

  const Pipeline& pipeline() const noexcept;

  Frame frameContext(glm::mat4 view, glm::mat4 proj) const;

private:
  Texture m_texture;
  Descriptors m_descriptors;
  Pipeline m_pipeline;

  std::vector<Mesh> m_meshes;
  std::vector<DrawItem> m_drawItems;
};
