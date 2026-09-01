#include <catch2/catch_test_macros.hpp>

#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/massscale/massscale.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/serialize/componentjson/componentjson.h>

#include <algorithm>
#include <string>

namespace {

using dunya::objectmodel::Deformable;
using dunya::objectmodel::Entity;
using dunya::objectmodel::MassScale;
using dunya::objectmodel::Pose;
using dunya::objectmodel::SdfGrid;
using dunya::objectmodel::World;
using dunya::serialize::authoredComponentNames;
using dunya::serialize::readComponent;

SdfGrid blank() {
  SdfGrid grid{};

  grid.resolution = glm::uvec3(8u);

  return grid;
}

Entity placed(World& world) {
  Pose pose{};

  pose.position = glm::vec3(1.5f, -2.25f, 0.5f);

  return world.createSdfGrid(pose, blank());
}

}

TEST_CASE("a pose reads as the json the world file stores", "[componentjson]") {
  World world;

  std::string json;

  REQUIRE(readComponent(world, placed(world), "Pose", json));
  REQUIRE(json == R"({"position":[1.5,-2.25,0.5],"rotation":[1,0,0,0]})");
}

TEST_CASE("a grid reads only its authored field", "[componentjson]") {
  World world;

  std::string json;

  REQUIRE(readComponent(world, placed(world), "SdfGrid", json));
  REQUIRE(json == R"({"resolution":[8,8,8]})");
}

TEST_CASE("an empty component reads as an empty object", "[componentjson]") {
  World world;

  const Entity entity = placed(world);

  world.emplaceAuthored<Deformable>(entity, Deformable{});

  std::string json;

  REQUIRE(readComponent(world, entity, "Deformable", json));
  REQUIRE(json == "{}");
}

TEST_CASE("a component the entity lacks is refused", "[componentjson]") {
  World world;

  std::string json = "untouched";

  REQUIRE(!readComponent(world, placed(world), "MassScale", json));
  REQUIRE(json == "untouched");
}

TEST_CASE(
  "a name that is not an authored component is refused",
  "[componentjson]"
) {
  World world;

  std::string json = "untouched";

  REQUIRE(!readComponent(world, placed(world), "BakedVolume", json));
  REQUIRE(json == "untouched");
}

TEST_CASE("every authored component is named once", "[componentjson]") {
  const auto names = authoredComponentNames();

  REQUIRE(names.size() == 10);

  auto sorted = names;

  std::sort(sorted.begin(), sorted.end());

  REQUIRE(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());

  const auto has = [&](std::string_view name) {
    return std::find(names.begin(), names.end(), name) != names.end();
  };

  REQUIRE(has("Pose"));
  REQUIRE(has("SdfGrid"));
  REQUIRE(has("Material"));
  REQUIRE(has("Deformable"));
  REQUIRE(has("DirectionalLight"));
  REQUIRE(has("Environment"));
}

TEST_CASE(
  "a component the entity gains later becomes readable",
  "[componentjson]"
) {
  World world;

  const Entity entity = placed(world);

  std::string json;

  REQUIRE(!readComponent(world, entity, "MassScale", json));

  world.emplaceOrReplace<MassScale>(entity, MassScale{2.5f});

  REQUIRE(readComponent(world, entity, "MassScale", json));
  REQUIRE(json == R"({"factor":2.5})");
}
