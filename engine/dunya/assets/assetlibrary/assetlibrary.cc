#include "assetlibrary.ih"

namespace dunya::assets {

AssetLibrary::AssetLibrary(
  const dunya::gpu::Context& context,
  const std::filesystem::path& projectRoot
)
    : m_samplers(createSamplers(context.device())),
      m_textures(createTextures(context.device())) {
  loadProject(context.device(), projectRoot);
}

void AssetLibrary::loadProject(
  const dunya::gpu::Device& device,
  const std::filesystem::path& projectRoot
) {
  std::optional<dunya::serialize::Project> project =
    dunya::serialize::Project::open(projectRoot);

  if (!project.has_value()) {
    throw std::runtime_error(
      "AssetLibrary: no project at " + projectRoot.string()
    );
  }

  for (const dunya::serialize::AssetEntry* entry : project->ofType("texture")) {
    const std::string path = (project->root() / entry->path).string();

    static_cast<void>(loadTexture(device, entry->id, path.c_str()));
  }

  for (const dunya::serialize::AssetEntry* entry :
       project->ofType("material")) {
    const std::optional<std::string> text =
      dunya::serialize::readText(project->root() / entry->path);

    if (!text.has_value()) {
      throw std::runtime_error("AssetLibrary: cannot read " + entry->path);
    }

    dunya::serialize::StoredMaterial stored{};

    if (!dunya::serialize::readMaterial(*text, stored)) {
      throw std::runtime_error("AssetLibrary: cannot parse " + entry->path);
    }

    static_cast<void>(addMaterial(entry->id, stored));
  }

  for (const dunya::serialize::AssetEntry* entry : project->ofType("mesh")) {
    const std::string path = (project->root() / entry->path).string();

    static_cast<void>(loadMesh(device, entry->id, path.c_str()));
  }
}

uint32_t AssetLibrary::loadTexture(
  const dunya::gpu::Device& device,
  dunya::core::AssetId id,
  const char* path
) {
  const uint32_t index = static_cast<uint32_t>(m_textures.size());

  m_assets.bind<dunya::gpu::Texture>(id, index);

  m_textures.emplace_back(dunya::gpu::Texture(device, path));

  return index;
}

uint32_t AssetLibrary::addMaterial(
  dunya::core::AssetId id,
  const dunya::serialize::StoredMaterial& stored
) {
  const auto slot =
    [this](const dunya::serialize::StoredTextureSlot& held, uint32_t fallback) {
      if (held.texture == dunya::core::INVALID_ASSET) {
        return fallback;
      }

      const uint32_t index = m_assets.index<dunya::gpu::Texture>(held.texture);

      if (index == dunya::core::UNBOUND_ASSET) {
        throw std::runtime_error(
          "AssetLibrary: a material names a texture the project has not bound"
        );
      }

      return index;
    };

  dunya::renderer::MaterialRecord record{};

  record.baseColor = stored.baseColor;
  record.emissive = stored.emissive;

  record.metallic = stored.metallic;
  record.roughness = stored.roughness;
  record.normalScale = stored.normalScale;
  record.occlusionStrength = stored.occlusionStrength;
  record.alphaCutoff = stored.alphaCutoff;
  record.flags = stored.flags;

  record.baseColorTexture =
    slot(stored.baseColorTexture, dunya::core::TEXTURE_WHITE);
  record.baseColorSampler = stored.baseColorTexture.sampler;

  record.metallicRoughnessTexture =
    slot(stored.metallicRoughnessTexture, dunya::core::TEXTURE_WHITE);
  record.metallicRoughnessSampler = stored.metallicRoughnessTexture.sampler;

  record.normalTexture =
    slot(stored.normalTexture, dunya::core::TEXTURE_FLAT_NORMAL);
  record.normalSampler = stored.normalTexture.sampler;

  record.occlusionTexture =
    slot(stored.occlusionTexture, dunya::core::TEXTURE_WHITE);
  record.occlusionSampler = stored.occlusionTexture.sampler;

  record.emissiveTexture =
    slot(stored.emissiveTexture, dunya::core::TEXTURE_BLACK);
  record.emissiveSampler = stored.emissiveTexture.sampler;

  const uint32_t index = static_cast<uint32_t>(m_materials.size());

  m_assets.bind<dunya::objectmodel::Material>(id, index);

  m_materials.push_back(record);

  return index;
}

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

const std::vector<dunya::renderer::MeshBuffers>& AssetLibrary::
  meshBuffers() const noexcept {
  return m_meshes;
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
  textures.reserve(dunya::core::RESERVED_TEXTURES);

  textures.emplace_back(device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, white.data());
  textures.emplace_back(
    device,
    1,
    1,
    VK_FORMAT_R8G8B8A8_UNORM,
    flatNormal.data()
  );
  textures.emplace_back(device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, black.data());

  return textures;
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

const dunya::core::AssetDatabase& AssetLibrary::assets() const noexcept {
  return m_assets;
}

}
