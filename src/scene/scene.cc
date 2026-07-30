#include "scene.ih"

Scene::Scene(const Context& context) : m_primitives(createPrimitives()) {
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

void Scene::augmentFrameContext(Frame& frameContext) const {
  std::span<const DrawItem> data(m_drawItems);
  std::span<const Mesh> meshes(m_meshes);
  std::span<const Primitive> primitives(m_primitives);
  frameContext.drawItems = data;
  frameContext.meshes = meshes;
  frameContext.primitives = primitives;
}

const std::vector<Primitive>& Scene::primitives() const noexcept {
  return m_primitives;
}

std::vector<Primitive> Scene::createPrimitives() {
  std::vector<Primitive> primitives;

  Primitive sphere{};
  sphere.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.45f, 0.0f)));
  sphere.shape = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
  sphere.shapeConfig = glm::uvec4(0, 0, 0, 0);

  Primitive box{};
  box.inverseModel = glm::inverse(
    glm::translate(glm::mat4(1.0f), glm::vec3(1.8f, 0.0f, 0.0f))
    * glm::rotate(
      glm::mat4(1.0f),
      glm::radians(30.0f),
      glm::vec3(0.0f, 1.0f, 0.0f)
    )
  );
  box.shape = glm::vec4(0.5f, 0.5f, 0.5f, 0.4f);
  box.shapeConfig = glm::uvec4(1, 0, 1, 0);

  Primitive plane{};
  plane.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -2.0f, 0.0f)));
  plane.shape = glm::vec4(0.0f);
  plane.shapeConfig = glm::uvec4(2, 1, 0, 0);

  primitives.push_back(sphere);
  primitives.push_back(box);
  primitives.push_back(plane);

  return primitives;
}
