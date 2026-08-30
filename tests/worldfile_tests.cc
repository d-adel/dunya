#include <catch2/catch_test_macros.hpp>

#include <dunya/serialize/worldfile/worldfile.h>

#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/massscale/massscale.h>
#include <dunya/objectmodel/component/staticbody/staticbody.h>
#include <dunya/objectmodel/component/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/component/deformed/deformed.h>
#include <dunya/objectmodel/component/rigidbody/rigidbody.h>
#include <dunya/objectmodel/component/lens/lens.h>

#include <vector>

using dunya::core::AssetDatabase;
using dunya::objectmodel::World;
using dunya::serialize::captureWorld;
using dunya::serialize::readWorld;
using dunya::serialize::restoreWorld;
using dunya::serialize::StoredWorld;
using dunya::serialize::WORLD_VERSION;
using dunya::serialize::writeWorld;

namespace {

constexpr dunya::core::AssetId STONE = 0xA1B2C3D4E5F60718ULL;
constexpr dunya::core::AssetId TIMBER = 0x1807F6E5D4C3B2A1ULL;

AssetDatabase inOrder() {
  AssetDatabase assets;
  assets.bind<dunya::objectmodel::Material>(STONE, 0u);
  assets.bind<dunya::objectmodel::Material>(TIMBER, 1u);

  return assets;
}

AssetDatabase reordered() {
  AssetDatabase assets;
  assets.bind<dunya::objectmodel::Material>(STONE, 1u);
  assets.bind<dunya::objectmodel::Material>(TIMBER, 0u);

  return assets;
}

dunya::objectmodel::Entity authorBox(World& world, uint32_t material) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(65u);

  dunya::objectmodel::Pose pose{};
  pose.position = glm::vec3(1.0f, 2.0f, 3.0f);

  const dunya::objectmodel::Entity entity = world.createSdfGrid(pose, grid);

  REQUIRE(world.addPrimitive(
    entity,
    dunya::field::makeBox(
      glm::vec3(0.0f),
      glm::vec3(0.45f),
      0.0f,
      glm::vec3(0.0f, 1.0f, 0.0f),
      material
    )
  ));

  return entity;
}

}

TEST_CASE("entities keep the order they were authored in", "[worldfile]") {
  World authored;

  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(65u);

  std::vector<float> heights;

  for (uint32_t index = 0u; index != 5u; ++index) {
    dunya::objectmodel::Pose pose{};
    pose.position = glm::vec3(0.0f, float(index), 0.0f);

    const dunya::objectmodel::Entity entity =
      authored.createSdfGrid(pose, grid);

    REQUIRE(authored.addPrimitive(
      entity,
      dunya::field::makeBox(glm::vec3(0.0f), glm::vec3(0.45f))
    ));

    heights.push_back(float(index));
  }

  const StoredWorld stored = captureWorld(authored, inOrder());

  REQUIRE(stored.entities.size() == heights.size());

  for (size_t at = 0u; at != heights.size(); ++at) {
    REQUIRE(stored.entities[at].pose.has_value());
    REQUIRE(stored.entities[at].pose->position.y == heights[at]);
  }

  World reloaded;
  REQUIRE(restoreWorld(stored, reloaded, inOrder()));

  const StoredWorld again = captureWorld(reloaded, inOrder());

  REQUIRE(again.entities.size() == stored.entities.size());

  for (size_t at = 0u; at != again.entities.size(); ++at) {
    REQUIRE(again.entities[at].pose->position.y == heights[at]);
  }
}

TEST_CASE("a world survives a round trip through text", "[worldfile]") {
  World authored;

  const dunya::objectmodel::Entity box = authorBox(authored, 0u);
  authored.addStaticBody(box);
  authored.emplaceOrReplace<dunya::objectmodel::Deformable>(
    box,
    dunya::objectmodel::Deformable{}
  );
  authored.emplaceOrReplace<dunya::objectmodel::MassScale>(
    box,
    dunya::objectmodel::MassScale{0.25f}
  );

  const AssetDatabase assets = inOrder();

  const std::string text = writeWorld(captureWorld(authored, assets));

  REQUIRE(!text.empty());

  StoredWorld stored{};
  REQUIRE(readWorld(text, stored));

  World loaded;
  REQUIRE(restoreWorld(stored, loaded, assets));

  REQUIRE(loaded.sdfGrids().size() == 1);

  const dunya::objectmodel::Entity back = loaded.sdfGrids()[0];
  const entt::registry& registry = loaded.registry();

  REQUIRE(registry.get<dunya::objectmodel::Pose>(back).position.y == 2.0f);
  REQUIRE(registry.all_of<dunya::objectmodel::StaticBody>(back));
  REQUIRE(registry.all_of<dunya::objectmodel::Deformable>(back));
  REQUIRE(registry.get<dunya::objectmodel::MassScale>(back).factor == 0.25f);
  REQUIRE(loaded.primitiveCount(back) == 1u);
}

TEST_CASE(
  "a material follows its id when the palette reorders",
  "[worldfile]"
) {
  World authored;

  static_cast<void>(authorBox(authored, 0u));

  const AssetDatabase writing = inOrder();
  const AssetDatabase readingBack = reordered();

  const StoredWorld stored = captureWorld(authored, writing);

  World loaded;
  REQUIRE(restoreWorld(stored, loaded, readingBack));

  const dunya::objectmodel::Entity back = loaded.sdfGrids()[0];

  REQUIRE(loaded.primitives(back)[0].shapeConfig[1] == 1u);
}

TEST_CASE("an unknown material refuses to load", "[worldfile]") {
  World authored;

  static_cast<void>(authorBox(authored, 0u));

  const AssetDatabase writing = inOrder();

  const StoredWorld stored = captureWorld(authored, writing);

  AssetDatabase empty;
  empty.bind<dunya::objectmodel::Material>(TIMBER, 0u);

  World loaded;
  REQUIRE(!restoreWorld(stored, loaded, empty));
}

TEST_CASE("a file from another version refuses to load", "[worldfile]") {
  StoredWorld stored{};
  stored.version = dunya::serialize::WORLD_VERSION + 1u;

  World loaded;
  const AssetDatabase none;

  REQUIRE(!restoreWorld(stored, loaded, none));
}

TEST_CASE("the grid is refitted on load, not read", "[worldfile]") {
  World authored;

  static_cast<void>(authorBox(authored, 0u));

  const AssetDatabase assets = inOrder();

  const std::string text = writeWorld(captureWorld(authored, assets));

  REQUIRE(text.find("voxelSize") == std::string::npos);
  REQUIRE(text.find("origin") == std::string::npos);

  StoredWorld stored{};
  REQUIRE(readWorld(text, stored));

  World loaded;
  REQUIRE(restoreWorld(stored, loaded, assets));

  const dunya::objectmodel::Entity back = loaded.sdfGrids()[0];

  const dunya::objectmodel::SdfGrid& grid =
    loaded.registry().get<dunya::objectmodel::SdfGrid>(back);

  REQUIRE(grid.resolution == glm::uvec3(65u));
  REQUIRE(grid.voxelSize.x > 0.0f);
}

TEST_CASE("a mesh and a field share one entity list", "[worldfile]") {
  World authored;

  static_cast<void>(authorBox(authored, 0u));

  AssetDatabase assets = inOrder();
  assets.bind<dunya::objectmodel::Mesh>(TIMBER, 4u);

  static_cast<void>(authored.createMesh(
    dunya::objectmodel::Pose{glm::vec3(5.0f, 6.0f, 7.0f), glm::quat()},
    dunya::objectmodel::Mesh{4u},
    dunya::objectmodel::Material{1u}
  ));

  const std::string text = writeWorld(captureWorld(authored, assets));

  StoredWorld stored{};
  REQUIRE(readWorld(text, stored));

  REQUIRE(stored.entities.size() == 2);

  World loaded;
  REQUIRE(restoreWorld(stored, loaded, assets));

  REQUIRE(loaded.sdfGrids().size() == 1);
  REQUIRE(loaded.meshes().size() == 1);

  const dunya::objectmodel::Entity mesh = loaded.meshes()[0];

  REQUIRE(loaded.registry().get<dunya::objectmodel::Mesh>(mesh).index == 4u);
  REQUIRE(
    loaded.registry().get<dunya::objectmodel::Pose>(mesh).position.z == 7.0f
  );
}

TEST_CASE("authored and self contained are separate questions", "[worldfile]") {
  static_assert(dunya::objectmodel::Authored<dunya::objectmodel::SdfGrid>);
  static_assert(
    !dunya::objectmodel::SelfContained<dunya::objectmodel::SdfGrid>
  );

  static_assert(dunya::objectmodel::Authored<dunya::objectmodel::StaticBody>);
  static_assert(
    !dunya::objectmodel::SelfContained<dunya::objectmodel::StaticBody>
  );

  SUCCEED("checked at compile time");
}

TEST_CASE("derived state cannot reach a world file", "[worldfile]") {
  static_assert(!dunya::objectmodel::Authored<dunya::objectmodel::RigidBody>);
  static_assert(!dunya::objectmodel::Authored<dunya::objectmodel::BakedVolume>);
  static_assert(!dunya::objectmodel::Authored<dunya::objectmodel::Deformed>);
  static_assert(
    !dunya::objectmodel::Authored<dunya::objectmodel::SdfPrimitiveRange>
  );

  SUCCEED("checked at compile time");
}

TEST_CASE("one entity can be both a field and a mesh", "[worldfile]") {
  World authored;

  const dunya::objectmodel::Entity both = authorBox(authored, 0u);

  AssetDatabase assets = inOrder();
  assets.bind<dunya::objectmodel::Mesh>(TIMBER, 4u);

  authored.emplaceOrReplace<dunya::objectmodel::Mesh>(
    both,
    dunya::objectmodel::Mesh{4u}
  );
  authored.emplaceOrReplace<dunya::objectmodel::Material>(
    both,
    dunya::objectmodel::Material{1u}
  );

  const StoredWorld stored = captureWorld(authored, assets);

  REQUIRE(stored.entities.size() == 1);

  World loaded;
  REQUIRE(restoreWorld(stored, loaded, assets));

  REQUIRE(loaded.sdfGrids().size() == 1);
  REQUIRE(loaded.meshes().size() == 1);
  REQUIRE(loaded.sdfGrids()[0] == loaded.meshes()[0]);
}

TEST_CASE("a file written by an older build still loads", "[worldfile]") {
  World authored;

  static_cast<void>(authorBox(authored, 0u));

  const AssetDatabase assets = inOrder();

  StoredWorld stored = captureWorld(authored, assets);
  stored.version = WORLD_VERSION - 1u;

  World loaded;
  REQUIRE(restoreWorld(stored, loaded, assets));
  REQUIRE(loaded.sdfGrids().size() == 1);
}

TEST_CASE("a key this build never heard of is skipped", "[worldfile]") {
  const std::string text = R"({
    "version": 1,
    "entities": [{
      "pose": { "position": [1, 2, 3], "rotation": [1, 0, 0, 0] },
      "grid": { "resolution": [65, 65, 65] },
      "spookiness": { "level": 11 },
      "primitives": []
    }]
  })";

  StoredWorld stored{};
  REQUIRE(readWorld(text, stored));

  World loaded;
  const AssetDatabase assets = inOrder();

  REQUIRE(restoreWorld(stored, loaded, assets));
  REQUIRE(loaded.sdfGrids().size() == 1);
}

TEST_CASE("a camera is a pose and a lens", "[worldfile]") {
  World authored;

  const dunya::objectmodel::Entity eye = authored.createAuthored();

  authored.emplaceAuthored<dunya::objectmodel::Pose>(
    eye,
    dunya::objectmodel::Pose{glm::vec3(0.0f, 3.0f, 12.0f), glm::quat()}
  );

  authored.emplaceAuthored<dunya::objectmodel::Lens>(
    eye,
    dunya::objectmodel::Lens{55.0f, 0.05f, 500.0f}
  );

  const AssetDatabase assets = inOrder();

  const std::string text = writeWorld(captureWorld(authored, assets));

  StoredWorld stored{};
  REQUIRE(readWorld(text, stored));
  REQUIRE(stored.entities.size() == 1);

  World loaded;
  REQUIRE(restoreWorld(stored, loaded, assets));

  const auto view =
    loaded.registry()
      .view<const dunya::objectmodel::Lens, const dunya::objectmodel::Pose>();

  REQUIRE(view.size_hint() == 1);

  for (const dunya::objectmodel::Entity entity : view) {
    const auto& lens = view.get<const dunya::objectmodel::Lens>(entity);
    const auto& pose = view.get<const dunya::objectmodel::Pose>(entity);

    REQUIRE(lens.verticalFov == 55.0f);
    REQUIRE(lens.nearPlane == 0.05f);
    REQUIRE(lens.farPlane == 500.0f);
    REQUIRE(pose.position.z == 12.0f);
  }
}
