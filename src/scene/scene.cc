#include "scene.ih"

Scene::Scene(const Context& context, const SwapChain& swapChain)
    : m_texture(context.device(), "textures/viking_room.png"),
      m_mesh(context.device(), "models/viking_room.obj"),
      m_descriptors(context.device(), m_texture),
      m_pipeline(context.device().vkDevice(), m_descriptors, swapChain) {}

const Mesh& Scene::mesh() const noexcept {
  return m_mesh;
}

const Descriptors& Scene::descriptors() const noexcept {
  return m_descriptors;
}

Descriptors& Scene::descriptors() {
  return m_descriptors;
}

const Pipeline& Scene::pipeline() const noexcept {
  return m_pipeline;
}
