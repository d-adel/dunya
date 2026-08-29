#include "scene.ih"

using dunya::field::Primitive;

namespace {

// Half the side of a stacked box, and the gap it is dropped from. The gap is
// there so the stack settles into contact rather than starting in it: a stack
// that has to find its own rest is the one M18 is about.
constexpr float BOX_HALF = 0.3f;
constexpr float BOX_GAP = 0.01f;
constexpr uint32_t BOX_COLUMNS = 4u;
constexpr uint32_t BOX_ROWS = 5u;

// Coarser than the ball, and it can be: a box meets the ground face to face,
// so a whole plane of bricks is in contact however few there are. A ball meets
// it at a point, and there the seed spacing has to beat the contact patch.
constexpr uint32_t BOX_RESOLUTION = 65u;

// The plane the stack sits on: its top face lands on the entity origin, so a
// box resting on it sits at exactly BOX_HALF above the plane entity.
constexpr float GROUND_Y = -2.0f;
constexpr float GROUND_HALF_THICKNESS = 0.5f;

constexpr float PROJECTILE_RADIUS = 0.35f;

// Its own entry in the material list, so the ball is not the colour of the
// plane it rolls across.
constexpr uint32_t PROJECTILE_MATERIAL = 4u;

// The full grid, unlike the boxes. A sphere resting on a plane touches it over
// a patch of about sqrt(2 r d) - a few centimetres - and a contact seed sits at
// a brick centre, so a grid whose bricks are wider than that patch has no seed
// where the ball actually touches, and the ball falls through the floor.
constexpr uint32_t PROJECTILE_RESOLUTION = dunya::core::FIELD_GRID_RESOLUTION;

}  // namespace

Scene::Scene(
  const dunya::gpu::Context& context,
  dunya::objectmodel::World& world
)
    : m_materials(createMaterials()),
      m_samplers(createSamplers(context.device())),
      m_textures(createTextures(context.device())),
      m_world(world) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  // The wall. Untagged, so each box becomes a dynamic body at Play, and on its
  // own coarser grid: twenty full ones would be twenty seconds of baking.
  dunya::objectmodel::SdfGrid boxGrid{};
  boxGrid.resolution = glm::uvec3(BOX_RESOLUTION);

  const float pitch = 2.0f * BOX_HALF + BOX_GAP;
  const float firstColumn = -0.5f * float(BOX_COLUMNS - 1u) * pitch;

  dunya::objectmodel::Pose pose{};

  for (uint32_t row = 0u; row != BOX_ROWS; ++row) {
    for (uint32_t column = 0u; column != BOX_COLUMNS; ++column) {
      pose.position = glm::vec3(
        firstColumn + float(column) * pitch,
        GROUND_Y + BOX_HALF + float(row) * pitch,
        0.0f
      );

      const dunya::objectmodel::Entity boxEntity =
        m_world.createField(pose, boxGrid);

      addPrimitive(
        boxEntity,
        dunya::field::makeBox(glm::vec3(0.0f), glm::vec3(BOX_HALF)),
        "a stacked box"
      );
    }
  }

  // The plane. Its box sits half a thickness below the entity origin, which
  // puts the surface everything rests on at GROUND_Y exactly. Created after
  // the wall so that a stress carve, which takes the first field there is,
  // lands on a box rather than on twenty square metres of floor.
  pose.position = glm::vec3(0.0f, GROUND_Y, 0.0f);

  const dunya::objectmodel::Entity planeEntity =
    m_world.createField(pose, grid);

  addPrimitive(
    planeEntity,
    dunya::field::makeBox(
      glm::vec3(0.0f, -GROUND_HALF_THICKNESS, 0.0f),
      glm::vec3(10.0f, GROUND_HALF_THICKNESS, 10.0f),
      0.0f,
      glm::vec3(0.0f, 1.0f, 0.0f),
      1
    ),
    "the ground plane"
  );

  m_world.addStaticBody(planeEntity);

  // Baked once, here, so pressing the key does not. A full-resolution bake is
  // better than a second of stall per shot, and every ball is this same sphere.
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

  // Its own grid, and a small one. Every field entity bakes its whole grid on
  // the frame it appears, so a shot on a full-resolution one would stall the
  // loop for a second - and a sphere has nothing a finer grid would resolve.
  shot.grid.resolution = glm::uvec3(PROJECTILE_RESOLUTION);

  shot.shape = dunya::field::makeSphere(
    glm::vec3(0.0f),
    PROJECTILE_RADIUS,
    PROJECTILE_MATERIAL
  );

  // Fast enough that a step carries the ball further than its own radius,
  // which is the case the swept path exists for.
  shot.speed = 22.0f;
  shot.height = 2.5f;

  shot.mass = 150.0f;

  // The middle of the stack, so a hit topples it rather than sliding the
  // bottom box out from under it.
  shot.aimAt =
    glm::vec3(0.0f, GROUND_Y + BOX_HALF + (2.0f * BOX_HALF + BOX_GAP), 0.0f);

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

  dunya::renderer::MaterialRecord projectile = neutral;
  projectile.baseColor = glm::vec4(0.15f, 0.65f, 0.75f, 1.0f);

  return {fieldSphere, fieldPlane, vikingRoom, checker, projectile};
}

const dunya::field::SampledField& Scene::projectileField() const noexcept {
  return m_projectileField;
}

void Scene::frame(dunya::objectmodel::Camera& camera) const {
  // Looking down on the stack rather than at it: the shot comes from the
  // camera, so the view has to show the plane the balls travel across.
  camera.place(glm::vec3(0.0f, 3.4f, 7.5f), 0.0f, glm::radians(-27.0f));
}
