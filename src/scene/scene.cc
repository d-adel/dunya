#include "scene.ih"

Scene::Scene(const Context& context, const SwapChain& swapChain)
    : m_texture(context.device(), "textures/viking_room.png"),
      m_descriptors(context.device(), m_texture),
      m_pipeline(context.device().vkDevice(), m_descriptors, swapChain) {
  glm::mat4 model = glm::rotate(
    glm::mat4(1.0f),
    glm::radians(-90.0f),
    glm::vec3(1.0f, 0.0f, 0.0f)
  );

  glm::mat4 model2 =
    glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f)) * model;
  m_meshes.emplace_back(Mesh(context.device(), "models/viking_room.obj"));
  m_meshes.emplace_back(Mesh(context.device(), "models/viking_room.obj"));
  m_drawItems.emplace_back(DrawItem({0, model}));
  m_drawItems.emplace_back(DrawItem({0, model2}));
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

Frame Scene::frameContext(glm::mat4 view, glm::mat4 proj) const {
  std::span<const DrawItem> data(m_drawItems);
  std::span<const Mesh> meshes(m_meshes);
  return Frame{view, proj, data, meshes};
}
