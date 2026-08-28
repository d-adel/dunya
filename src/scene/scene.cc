#include "scene.ih"

using dunya::field::Primitive;

Scene::Scene(
  const dunya::gpu::Context& context,
  dunya::objectmodel::World& world
)
    : m_materials(createMaterials()),
      m_samplers(createSamplers(context.device())),
      m_textures(createTextures(context.device())),
      m_world(world) {
  // Both meshes are the same room at the same orientation; only the placement
  // and the material differ. The rotation is what DrawItem's matrix carried.
  const glm::quat rotation =
    glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

  m_meshes.emplace_back(
    dunya::renderer::MeshBuffers(context.device(), "models/viking_room.obj")
  );

  m_world.createMesh(
    dunya::objectmodel::Pose{glm::vec3(0.0f), rotation},
    dunya::objectmodel::Mesh{0},
    dunya::objectmodel::Material{2}
  );
  m_world.createMesh(
    dunya::objectmodel::Pose{glm::vec3(0.0f, 0.0f, -2.0f), rotation},
    dunya::objectmodel::Mesh{0},
    dunya::objectmodel::Material{3}
  );

  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  dunya::objectmodel::Pose pose{};
  pose.position = glm::vec3(1.0f, 0.45f, 0.0f);
  const dunya::objectmodel::Entity fieldEntity =
    m_world.createField(pose, grid);
  addInitialPrimitives(fieldEntity);

  pose.position = glm::vec3(0.0f, -2.0f, 0.0f);
  const dunya::objectmodel::Entity planeEntity =
    m_world.createField(pose, grid);
  if (!m_world.addPrimitive(
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
      )) {
    throw std::runtime_error(

      "Scene: the ground plane box did not fit the primitive arena"

    );
  }

  // The body M18 drops onto the ground above. A field object rather than a
  // mesh, because D1 says every physical body is a field.
  pose.position = glm::vec3(-1.5f, 0.4f, 0.0f);
  const dunya::objectmodel::Entity bodyEntity = m_world.createField(pose, grid);

  if (!m_world.addPrimitive(
        bodyEntity,
        dunya::field::makeSphere(glm::vec3(0.0f), 0.5f)
      )) {
    throw std::runtime_error(
      "Scene: the drop sphere did not fit the primitive arena"
    );
  }
}

void Scene::augmentFrameContext(
  dunya::renderer::Frame& frameContext,
  const dunya::objectmodel::World& world
) {
  const entt::registry& registry = world.registry();

  // Packed here rather than stored, because a mesh record is per-frame data
  // derived from three components. A member so the span outlives the call.
  m_meshRecords.clear();

  for (dunya::objectmodel::Entity entity : world.meshes()) {
    m_meshRecords.push_back(
      {registry.get<dunya::objectmodel::Mesh>(entity).index,
       registry.get<dunya::objectmodel::Material>(entity).index,
       dunya::objectmodel::model(
         registry.get<dunya::objectmodel::Pose>(entity)
       )}
    );
  }

  frameContext.meshRecords = m_meshRecords;
  frameContext.meshes = m_meshes;
  frameContext.primitives = world.pool();
}

const std::vector<dunya::renderer::MaterialRecord>& Scene::
  materials() const noexcept {
  return m_materials;
}

const std::vector<dunya::gpu::Texture>& Scene::textures() const noexcept {
  return m_textures;
}

const std::vector<dunya::gpu::Sampler>& Scene::samplers() const noexcept {
  return m_samplers;
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

std::vector<dunya::renderer::MaterialRecord> Scene::createMaterials() {
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

  return {fieldSphere, fieldPlane, vikingRoom, checker};
}

void Scene::addInitialPrimitives(dunya::objectmodel::Entity entity) {
  if (!m_world.addPrimitive(
        entity,
        dunya::field::makeSphere(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f)
      )) {
    throw std::runtime_error(

      "Scene: the field sphere did not fit the primitive arena"

    );
  }

  if (!m_world.addPrimitive(
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
      )) {
    throw std::runtime_error(

      "Scene: the initial carve box did not fit the primitive arena"

    );
  }
}
