#include "scene.ih"

using dunya::field::Primitive;

namespace {

constexpr float BOX_HALF = 0.45f;
constexpr float BOX_GAP = 0.01f;

constexpr uint32_t BOX_RESOLUTION = 65u;

constexpr float GROUND_Y = -2.0f;
constexpr float GROUND_HALF_THICKNESS = 0.5f;

constexpr float GROUND_MARGIN = 4.0f;

constexpr float PROJECTILE_RADIUS = 0.35f;

constexpr uint32_t PROJECTILE_MATERIAL = 4u;

constexpr uint32_t PROJECTILE_RESOLUTION = dunya::core::FIELD_GRID_RESOLUTION;

}

Scene::Scene(
  const dunya::gpu::Context& context,
  dunya::objectmodel::World& world,
  uint32_t wallColumns,
  uint32_t wallRows,
  uint32_t wallDepth
)
    : m_materials(createMaterials()),
      m_samplers(createSamplers(context.device())),
      m_textures(createTextures(context.device())),
      m_world(world) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  dunya::objectmodel::SdfGrid boxGrid{};
  boxGrid.resolution = glm::uvec3(BOX_RESOLUTION);

  constexpr uint32_t RESERVED_SLOTS = 2u + 24u;

  const uint32_t affordable =
    std::min(dunya::core::MAX_FIELD_VOLUMES, dunya::core::MAX_FIELD_RECORDS)
    - RESERVED_SLOTS;

  while (wallColumns * wallRows * wallDepth > affordable && wallDepth > 1u) {
    --wallDepth;
  }

  while (wallColumns * wallRows * wallDepth > affordable && wallRows > 1u) {
    --wallRows;
  }

  while (wallColumns * wallRows * wallDepth > affordable && wallColumns > 1u) {
    --wallColumns;
  }

  std::cout << "Wall " << wallColumns << "x" << wallRows << "x" << wallDepth
            << " (" << (wallColumns * wallRows * wallDepth) << " boxes, "
            << affordable << " affordable)\n";

  const float pitch = 2.0f * BOX_HALF + BOX_GAP;
  const float firstColumn = -0.5f * float(wallColumns - 1u) * pitch;

  const float frontZ = BOX_HALF;

  m_wallMinimum = glm::vec3(
    firstColumn - BOX_HALF,
    GROUND_Y,
    frontZ - float(wallDepth) * pitch
  );

  m_wallMaximum = glm::vec3(
    firstColumn + float(wallColumns - 1u) * pitch + BOX_HALF,
    GROUND_Y + 2.0f * BOX_HALF + float(wallRows - 1u) * pitch,
    frontZ
  );

  dunya::objectmodel::Pose pose{};

  for (uint32_t layer = 0u; layer != wallDepth; ++layer) {
    for (uint32_t row = 0u; row != wallRows; ++row) {
      for (uint32_t column = 0u; column != wallColumns; ++column) {
        pose.position = glm::vec3(
          firstColumn + float(column) * pitch,
          GROUND_Y + BOX_HALF + float(row) * pitch,
          -float(layer) * pitch
        );

        const dunya::objectmodel::Entity boxEntity =
          m_world.createField(pose, boxGrid);

        addPrimitive(
          boxEntity,
          dunya::field::makeBox(glm::vec3(0.0f), glm::vec3(BOX_HALF)),
          "a stacked box"
        );

        m_world.emplaceOrReplace<dunya::objectmodel::Deformable>(
          boxEntity,
          dunya::objectmodel::Deformable{}
        );
      }
    }
  }

  pose.position = glm::vec3(0.0f, GROUND_Y, 0.0f);

  const dunya::objectmodel::Entity planeEntity =
    m_world.createField(pose, grid);

  const glm::vec3 wallSpan = m_wallMaximum - m_wallMinimum;

  const float groundHalfWidth =
    std::max(10.0f, 0.5f * std::max(wallSpan.x, wallSpan.z) + GROUND_MARGIN);

  addPrimitive(
    planeEntity,
    dunya::field::makeBox(
      glm::vec3(0.0f, -GROUND_HALF_THICKNESS, 0.0f),
      glm::vec3(groundHalfWidth, GROUND_HALF_THICKNESS, groundHalfWidth),
      0.0f,
      glm::vec3(0.0f, 1.0f, 0.0f),
      1
    ),
    "the ground plane"
  );

  m_world.addStaticBody(planeEntity);

  m_world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    planeEntity,
    dunya::objectmodel::Deformable{}
  );

  m_deformable = planeEntity;

  const Projectile shot = projectile();
  const dunya::field::Aabb box = dunya::objectmodel::gridBox({&shot.shape, 1});

  m_projectileField = dunya::field::bake(
    std::span<const dunya::field::Primitive>(&shot.shape, 1),
    box.minimum,
    box.maximum,
    shot.grid.resolution
  );
}

Scene::Projectile Scene::projectile() const {
  Projectile shot;

  shot.grid.resolution = glm::uvec3(PROJECTILE_RESOLUTION);

  shot.shape = dunya::field::makeSphere(
    glm::vec3(0.0f),
    PROJECTILE_RADIUS,
    PROJECTILE_MATERIAL
  );

  shot.speed = 42.0f;

  shot.height = 0.5f * (m_wallMinimum.y + m_wallMaximum.y);

  shot.mass = 600.0f;

  shot.aimAt = 0.5f * (m_wallMinimum + m_wallMaximum);

  return shot;
}

void Scene::addPrimitive(
  dunya::objectmodel::Entity entity,
  const dunya::field::Primitive& primitive,
  const char* what
) {
  if (!m_world.addPrimitive(entity, primitive)) {
    throw std::runtime_error(
      std::string("Scene: ") + what + " did not fit the primitive arena"
    );
  }
}

void Scene::augmentFrameContext(
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

  dunya::renderer::MaterialRecord projectile = neutral;
  projectile.baseColor = glm::vec4(0.15f, 0.65f, 0.75f, 1.0f);

  return {fieldSphere, fieldPlane, vikingRoom, checker, projectile};
}

const dunya::field::SampledField& Scene::projectileField() const noexcept {
  return m_projectileField;
}

void Scene::frame(dunya::objectmodel::Camera& camera) const {
  const glm::vec3 span = m_wallMaximum - m_wallMinimum;
  const glm::vec3 centre = 0.5f * (m_wallMinimum + m_wallMaximum);

  constexpr float HALF_FOV = glm::radians(35.0f);

  const float reach = 0.5f * std::max(span.x, span.y) / std::tan(HALF_FOV);

  const float distance = reach + 3.0f;

  constexpr float PITCH = glm::radians(-20.0f);

  camera.place(
    glm::vec3(0.0f, centre.y + distance * -std::sin(PITCH), distance),
    0.0f,
    PITCH
  );
}

glm::vec3 Scene::wallPoint(float u, float v) const {
  constexpr float INSET = 0.15f;

  const glm::vec3 span = m_wallMaximum - m_wallMinimum;

  return glm::vec3(
    m_wallMinimum.x + span.x * glm::mix(INSET, 1.0f - INSET, u),
    m_wallMinimum.y + span.y * glm::mix(INSET, 1.0f - INSET, v),
    0.0f
  );
}

dunya::objectmodel::Entity Scene::deformable() const noexcept {
  return m_deformable;
}
