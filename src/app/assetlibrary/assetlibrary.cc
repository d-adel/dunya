#include "assetlibrary.ih"

AssetLibrary::AssetLibrary(const dunya::gpu::Context& context)
    : m_materials(createMaterials()),
      m_samplers(createSamplers(context.device())),
      m_textures(createTextures(context.device())) {}

uint32_t AssetLibrary::loadMesh(
  const dunya::gpu::Device& device,
  dunya::core::AssetId id,
  const char* path
) {
  const uint32_t index = static_cast<uint32_t>(m_meshes.size());

  m_assets.bind<dunya::objectmodel::Mesh>(id, index);

  m_meshes.emplace_back(dunya::renderer::MeshBuffers(device, path));

  return index;
}

void AssetLibrary::augmentFrameContext(
  dunya::renderer::Frame& frameContext,
  const dunya::objectmodel::World& world
) {
  const entt::registry& registry = world.registry();

  m_meshRecords.clear();

  for (dunya::objectmodel::Entity entity : world.meshes()) {
    m_meshRecords.push_back(
      {registry.get<dunya::objectmodel::Mesh>(entity).index,
       registry.get<dunya::objectmodel::Material>(entity).index,
       dunya::objectmodel::model(
         registry.all_of<dunya::objectmodel::RenderPose>(entity)
           ? registry.get<dunya::objectmodel::RenderPose>(entity).pose
           : registry.get<dunya::objectmodel::Pose>(entity)
       )}
    );
  }

  frameContext.meshRecords = m_meshRecords;
  frameContext.meshes = m_meshes;
  frameContext.primitives = world.pool();
}

const std::vector<dunya::renderer::MaterialRecord>& AssetLibrary::
  materials() const noexcept {
  return m_materials;
}

const std::vector<dunya::gpu::Texture>& AssetLibrary::
  textures() const noexcept {
  return m_textures;
}

const std::vector<dunya::gpu::Sampler>& AssetLibrary::
  samplers() const noexcept {
  return m_samplers;
}

std::vector<dunya::gpu::Sampler> AssetLibrary::createSamplers(
  const dunya::gpu::Device& device
) {
  std::vector<dunya::gpu::Sampler> samplers;
  samplers.reserve(3);

  samplers.emplace_back(device);

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

std::vector<dunya::gpu::Texture> AssetLibrary::createTextures(
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

std::vector<dunya::renderer::MaterialRecord> AssetLibrary::createMaterials() {
  dunya::renderer::MaterialRecord neutral{};
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

  dunya::renderer::MaterialRecord fieldSphere = neutral;
  fieldSphere.baseColor = glm::vec4(0.5f, 0.0f, 0.3f, 1.0f);

  dunya::renderer::MaterialRecord fieldPlane = neutral;
  fieldPlane.baseColor = glm::vec4(0.7f, 0.6f, 0.3f, 1.0f);

  dunya::renderer::MaterialRecord vikingRoom = neutral;
  vikingRoom.baseColorTexture = dunya::core::RESERVED_TEXTURES;

  dunya::renderer::MaterialRecord checker = neutral;
  checker.baseColorTexture = dunya::core::RESERVED_TEXTURES + 1;

  dunya::renderer::MaterialRecord projectile = neutral;
  projectile.baseColor = glm::vec4(0.15f, 0.65f, 0.75f, 1.0f);

  m_assets.bind<dunya::objectmodel::Material>(
    dunya::core::MATERIAL_FIELD_SPHERE,
    0u
  );
  m_assets.bind<dunya::objectmodel::Material>(
    dunya::core::MATERIAL_FIELD_PLANE,
    1u
  );
  m_assets.bind<dunya::objectmodel::Material>(
    dunya::core::MATERIAL_VIKING_ROOM,
    2u
  );
  m_assets.bind<dunya::objectmodel::Material>(
    dunya::core::MATERIAL_CHECKER,
    3u
  );
  m_assets.bind<dunya::objectmodel::Material>(
    dunya::core::MATERIAL_PROJECTILE,
    4u
  );

  return {fieldSphere, fieldPlane, vikingRoom, checker, projectile};
}

uint32_t AssetLibrary::materialIndex(dunya::core::AssetId id) const {
  const uint32_t index = m_assets.index<dunya::objectmodel::Material>(id);

  if (index == dunya::core::UNBOUND_ASSET) {
    throw std::runtime_error(
      "AssetLibrary: no material is bound under that asset id"
    );
  }

  return index;
}

uint32_t AssetLibrary::meshIndex(dunya::core::AssetId id) const {
  const uint32_t index = m_assets.index<dunya::objectmodel::Mesh>(id);

  if (index == dunya::core::UNBOUND_ASSET) {
    throw std::runtime_error(
      "AssetLibrary: no mesh is bound under that asset id"
    );
  }

  return index;
}

const dunya::core::AssetDatabase& AssetLibrary::assets() const noexcept {
  return m_assets;
}
