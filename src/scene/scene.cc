#include "scene.ih"

using dunya::field::Primitive;

Scene::Scene(const Context& context)
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
  m_meshes.emplace_back(Mesh(context.device(), "models/viking_room.obj"));
  m_meshes.emplace_back(Mesh(context.device(), "models/viking_room.obj"));
  m_drawItems.emplace_back(DrawItem({0, 2, model}));
  m_drawItems.emplace_back(DrawItem({0, 3, model2}));

  ObjectId objectId = addFieldObject(glm::vec3(1.0f, 0.45f, 0.0f));
  addInitialPrimitives(objectId);
  ObjectId planeId = addFieldObject(glm::vec3(0.0f, -2.0f, 0.0f));
  m_objectRegistry.addPrimitive(
    planeId,
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

bool Scene::addPrimitive(
  ObjectId objectId,
  const glm::vec3& centre,
  float radius,
  float blend,
  uint32_t material,
  uint32_t operation
) {
  if (!m_objectRegistry.contains(objectId)) {
    return false;
  }

  const dunya::field::Primitive primitive =
    dunya::field::makeSphere(centre, radius, material, operation, blend);

  return m_objectRegistry.addPrimitive(objectId, primitive);
}

ObjectId Scene::addFieldObject(glm::vec3 position) {
  FieldObject fieldObject{};

  fieldObject.position = position;
  fieldObject.resolution = glm::uvec3(FIELD_GRID_RESOLUTION);

  const ObjectId objectId = m_objectRegistry.addFieldObject(fieldObject);

  if (objectId == INVALID_OBJECT_ID) {
    return INVALID_OBJECT_ID;
  }

  return objectId;
}

void Scene::setVolumeIndex(ObjectId objectIndex, uint32_t volumeIndex) {
  m_objectRegistry.getFieldObject(objectIndex).volumeIndex = volumeIndex;
}

void Scene::setDirty(ObjectId objectIndex, bool value) {
  m_objectRegistry.getFieldObject(objectIndex).dirty = value;
}

void Scene::augmentFrameContext(Frame& frameContext) {
  std::span<const DrawItem> data(m_drawItems);
  std::span<const Mesh> meshes(m_meshes);

  frameContext.drawItems = data;
  frameContext.meshes = meshes;
  frameContext.fieldObjectIds = m_objectRegistry.fieldObjectIds();
  frameContext.primitives = m_objectRegistry.primitivePool();
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

const ObjectRegistry& Scene::registry() const {
  return m_objectRegistry;
}

ObjectRegistry& Scene::registry() {
  return m_objectRegistry;
}

std::vector<Sampler> Scene::createSamplers(const Device& device) {
  std::vector<Sampler> samplers;
  samplers.reserve(3);

  samplers.emplace_back(device);

  // A sampled field is clamped rather than repeated, and its material volume
  // holds ids, which cannot be filtered at all.
  samplers.emplace_back(
    device,
    SamplerSettings{
      VK_FILTER_LINEAR,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      false
    }
  );
  samplers.emplace_back(
    device,
    SamplerSettings{
      VK_FILTER_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      false
    }
  );

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

void Scene::addInitialPrimitives(ObjectId objectId) {
  m_objectRegistry.addPrimitive(
    objectId,
    dunya::field::makeSphere(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f)
  );

  m_objectRegistry.addPrimitive(
    objectId,
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
