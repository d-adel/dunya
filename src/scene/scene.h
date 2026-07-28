#pragma once

#include "swapchain/swapchain.h"
#include "texture/texture.h"
#include "mesh/mesh.h"
#include "descriptors/descriptors.h"
#include "pipeline/pipeline.h"

class Scene {
public:
  Scene(const Context& context, const SwapChain& swapChain);
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  const Mesh& mesh() const noexcept;

  const Descriptors& descriptors() const noexcept;
  Descriptors& descriptors();

  const Pipeline& pipeline() const noexcept;

private:
  Texture m_texture;
  Mesh m_mesh;
  Descriptors m_descriptors;
  Pipeline m_pipeline;
};
