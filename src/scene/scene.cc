#include "scene.ih"

Scene::Scene(const Context& context)
    : m_texture(context.device(), "textures/viking_room.png"),
      m_descriptors(context.device(), m_texture) {
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

void Scene::augmentFrameContext(Frame& frameContext) const {
  std::span<const DrawItem> data(m_drawItems);
  std::span<const Mesh> meshes(m_meshes);
  frameContext.drawItems = data;
  frameContext.meshes = meshes;
}
