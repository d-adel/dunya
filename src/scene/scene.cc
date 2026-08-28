#include "scene.ih"

using dunya::field::Primitive;

Scene::Scene(const dunya::gpu::Context& context)
    : m_materials(createMaterials()),
      m_samplers(createSamplers(context.device())),
      m_textures(createTextures(context.device())) {
  glm::mat4 model = glm::rotate(
    glm::mat4(1.0f),
    glm::radians(-90.0f),
    glm::vec3(1.0f, 0.0f, 0.0f)
  );

  glm::mat4 model2 =
    glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f)) * model;
  m_meshes.emplace_back(
    dunya::renderer::Mesh(context.device(), "models/viking_room.obj")
  );
  m_meshes.emplace_back(
    dunya::renderer::Mesh(context.device(), "models/viking_room.obj")
  );
  m_world.addDrawItem(dunya::objectmodel::DrawItem({0, 2, model}));
  m_world.addDrawItem(dunya::objectmodel::DrawItem({0, 3, model2}));

  dunya::objectmodel::FieldGrid grid{};
  grid.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  dunya::objectmodel::Pose pose{};
  pose.position = glm::vec3(1.0f, 0.45f, 0.0f);
  const dunya::objectmodel::Entity fieldEntity =
    m_world.addFieldObject(pose, grid);
  addInitialPrimitives(fieldEntity);

  pose.position = glm::vec3(0.0f, -2.0f, 0.0f);
  const dunya::objectmodel::Entity planeEntity =
    m_world.addFieldObject(pose, grid);
  m_world.addPrimitive(
    planeEntity,
    dunya::field::makeBox(
      glm::vec3(0.0f, -0.5f, 0.0f),
      glm::vec3(10.0f, 0.5f, 10.0f),
      glm::radians(0.0f),
      glm::vec3(0.0f, 1.0f, 0.0f),
      1,
      0,
      0.0f
    )
  );
}

void Scene::augmentFrameContext(dunya::renderer::Frame& frameContext) {
  std::span<const dunya::renderer::Mesh> meshes(m_meshes);

  frameContext.drawItems = m_world.drawItems();
  frameContext.meshes = meshes;
  frameContext.primitives = m_world.pool();
}

const std::vector<dunya::objectmodel::Material>& Scene::
  materials() const noexcept {
  return m_materials;
}

const std::vector<dunya::gpu::Texture>& Scene::textures() const noexcept {
  return m_textures;
}

const std::vector<dunya::gpu::Sampler>& Scene::samplers() const noexcept {
  return m_samplers;
}

const dunya::objectmodel::World& Scene::world() const noexcept {
  return m_world;
}

dunya::objectmodel::World& Scene::world() noexcept {
  return m_world;
}

std::vector<dunya::gpu::Sampler> Scene::createSamplers(
  const dunya::gpu::Device& device
) {
  std::vector<dunya::gpu::Sampler> samplers;
  samplers.reserve(3);

  samplers.emplace_back(device);

  // A sampled field is clamped rather than repeated, and its material volume
  // holds ids, which cannot be filtered at all.
  samplers.emplace_back(
    device,
    dunya::gpu::SamplerSettings{
      VK_FILTER_LINEAR,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      false
    }
  );
  samplers.emplace_back(
    device,
    dunya::gpu::SamplerSettings{
      VK_FILTER_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      false
    }
  );

  return samplers;
}

std::vector<dunya::gpu::Texture> Scene::createTextures(
  const dunya::gpu::Device& device
) {
  const std::array<uint8_t, 4> white{255, 255, 255, 255};
  const std::array<uint8_t, 4> flatNormal{128, 128, 255, 255};
  const std::array<uint8_t, 4> black{0, 0, 0, 255};

  std::vector<dunya::gpu::Texture> textures;
  textures.reserve(dunya::core::RESERVED_TEXTURES + 2);

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

std::vector<dunya::objectmodel::Material> Scene::createMaterials() {
  dunya::objectmodel::Material neutral{};
  neutral.baseColor = glm::vec4(1.0f);
  neutral.emissive = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  neutral.metallic = 0.0f;
  neutral.roughness = 1.0f;
  neutral.normalScale = 1.0f;
  neutral.occlusionStrength = 1.0f;
  neutral.alphaCutoff = 0.5f;
  neutral.flags = 0;
  neutral.baseColorTexture = dunya::core::TEXTURE_WHITE;
  neutral.baseColorSampler = dunya::core::SAMPLER_LINEAR_REPEAT;
  neutral.metallicRoughnessTexture = dunya::core::TEXTURE_WHITE;
  neutral.metallicRoughnessSampler = dunya::core::SAMPLER_LINEAR_REPEAT;
  neutral.normalTexture = dunya::core::TEXTURE_FLAT_NORMAL;
  neutral.normalSampler = dunya::core::SAMPLER_LINEAR_REPEAT;
  neutral.occlusionTexture = dunya::core::TEXTURE_WHITE;
  neutral.occlusionSampler = dunya::core::SAMPLER_LINEAR_REPEAT;
  neutral.emissiveTexture = dunya::core::TEXTURE_BLACK;
  neutral.emissiveSampler = dunya::core::SAMPLER_LINEAR_REPEAT;

  dunya::objectmodel::Material fieldSphere = neutral;
  fieldSphere.baseColor = glm::vec4(0.5f, 0.0f, 0.3f, 1.0f);

  dunya::objectmodel::Material fieldPlane = neutral;
  fieldPlane.baseColor = glm::vec4(0.7f, 0.6f, 0.3f, 1.0f);

  dunya::objectmodel::Material vikingRoom = neutral;
  vikingRoom.baseColorTexture = dunya::core::RESERVED_TEXTURES;

  dunya::objectmodel::Material checker = neutral;
  checker.baseColorTexture = dunya::core::RESERVED_TEXTURES + 1;

  return {fieldSphere, fieldPlane, vikingRoom, checker};
}

void Scene::addInitialPrimitives(dunya::objectmodel::Entity entity) {
  m_world.addPrimitive(
    entity,
    dunya::field::makeSphere(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f)
  );

  m_world.addPrimitive(
    entity,
    dunya::field::makeBox(
      glm::vec3(0.8f, -0.45f, 0.0f),
      glm::vec3(0.5f),
      glm::radians(30.0f),
      glm::vec3(0.0f, 1.0f, 0.0f),
      0,
      1,
      0.4f
    )
  );
}
