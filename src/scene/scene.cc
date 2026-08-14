#include "scene.ih"

Scene::Scene(const Context& context)
    : m_primitives(createPrimitives()),
      m_materials(createMaterials()),
      m_samplers(createSamplers(context.device())),
      m_textures(createTextures(context.device())) {
  glm::mat4 model = glm::rotate(
    glm::mat4(1.0f),
    glm::radians(-90.0f),
    glm::vec3(1.0f, 0.0f, 0.0f)
  );

  glm::mat4 model2 =
    glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f)) * model;
  m_meshes.emplace_back(Mesh(context.device(), "models/viking_room.obj"));
  m_meshes.emplace_back(Mesh(context.device(), "models/viking_room.obj"));
  m_drawItems.emplace_back(DrawItem({0, 2, model}));
  m_drawItems.emplace_back(DrawItem({0, 3, model2}));
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

const std::vector<Material>& Scene::materials() const noexcept {
  return m_materials;
}

const std::vector<Texture>& Scene::textures() const noexcept {
  return m_textures;
}

const std::vector<Sampler>& Scene::samplers() const noexcept {
  return m_samplers;
}

std::vector<Sampler> Scene::createSamplers(const Device& device) {
  std::vector<Sampler> samplers;
  samplers.reserve(1);

  samplers.emplace_back(device);

  return samplers;
}

std::vector<Texture> Scene::createTextures(const Device& device) {
  const std::array<uint8_t, 4> white{255, 255, 255, 255};
  const std::array<uint8_t, 4> flatNormal{128, 128, 255, 255};
  const std::array<uint8_t, 4> black{0, 0, 0, 255};

  std::vector<Texture> textures;
  textures.reserve(RESERVED_TEXTURES + 2);

  textures.emplace_back(device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, white.data());
  textures.emplace_back(
    device,
    1,
    1,
    VK_FORMAT_R8G8B8A8_UNORM,
    flatNormal.data()
  );
  textures.emplace_back(device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, black.data());

  textures.emplace_back(device, "textures/viking_room.png");
  textures.emplace_back(device, "textures/texture.jpg");

  return textures;
}

std::vector<Material> Scene::createMaterials() {
  Material neutral{};
  neutral.baseColor = glm::vec4(1.0f);
  neutral.emissive = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  neutral.metallic = 0.0f;
  neutral.roughness = 1.0f;
  neutral.normalScale = 1.0f;
  neutral.occlusionStrength = 1.0f;
  neutral.alphaCutoff = 0.5f;
  neutral.flags = 0;
  neutral.baseColorTexture = TEXTURE_WHITE;
  neutral.baseColorSampler = SAMPLER_LINEAR_REPEAT;
  neutral.metallicRoughnessTexture = TEXTURE_WHITE;
  neutral.metallicRoughnessSampler = SAMPLER_LINEAR_REPEAT;
  neutral.normalTexture = TEXTURE_FLAT_NORMAL;
  neutral.normalSampler = SAMPLER_LINEAR_REPEAT;
  neutral.occlusionTexture = TEXTURE_WHITE;
  neutral.occlusionSampler = SAMPLER_LINEAR_REPEAT;
  neutral.emissiveTexture = TEXTURE_BLACK;
  neutral.emissiveSampler = SAMPLER_LINEAR_REPEAT;

  Material fieldSphere = neutral;
  fieldSphere.baseColor = glm::vec4(0.5f, 0.0f, 0.3f, 1.0f);

  Material fieldPlane = neutral;
  fieldPlane.baseColor = glm::vec4(0.7f, 0.6f, 0.3f, 1.0f);

  Material vikingRoom = neutral;
  vikingRoom.baseColorTexture = RESERVED_TEXTURES;

  Material checker = neutral;
  checker.baseColorTexture = RESERVED_TEXTURES + 1;

  return {fieldSphere, fieldPlane, vikingRoom, checker};
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
