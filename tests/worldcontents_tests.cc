#include <catch2/catch_test_macros.hpp>

#include <dunya/objectmodel/component/material/material.h>
#include <dunya/objectmodel/component/mesh/mesh.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/rigidbody/rigidbody.h>
#include <dunya/objectmodel/worldcontents/worldcontents.h>
#include <dunya/objectmodel/world/world.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace {

using dunya::objectmodel::componentNames;
using dunya::objectmodel::Entity;
using dunya::objectmodel::liveEntities;
using dunya::objectmodel::Material;
using dunya::objectmodel::Mesh;
using dunya::objectmodel::Pose;
using dunya::objectmodel::SdfGrid;
using dunya::objectmodel::World;

SdfGrid blank() {
  SdfGrid grid{};

  grid.resolution = glm::uvec3(8u);

  return grid;
}

bool has(const std::vector<std::string>& names, const std::string& name) {
  return std::find(names.begin(), names.end(), name) != names.end();
}

}

TEST_CASE("every live entity is listed once, in creation order", "[contents]") {
  World world;

  const Entity first = world.createSdfGrid(Pose{}, blank());
  const Entity second = world.createSdfGrid(Pose{}, blank());
  const Entity third = world.createMesh(Pose{}, Mesh{}, Material{});

  const auto listed = liveEntities(world);

  REQUIRE(listed.size() == 3);
  REQUIRE(listed[0] == first);
  REQUIRE(listed[1] == second);
  REQUIRE(listed[2] == third);
}

TEST_CASE("a destroyed entity leaves the listing", "[contents]") {
  World world;

  const Entity kept = world.createSdfGrid(Pose{}, blank());
  const Entity gone = world.createSdfGrid(Pose{}, blank());

  REQUIRE(world.destroy(gone));

  const auto listed = liveEntities(world);

  REQUIRE(listed.size() == 1);
  REQUIRE(listed[0] == kept);
}

TEST_CASE("components are reported by their unqualified name", "[contents]") {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  const auto names = componentNames(world, entity);

  REQUIRE(has(names, "Pose"));
  REQUIRE(has(names, "SdfGrid"));
  REQUIRE(!has(names, "Mesh"));
}

TEST_CASE(
  "a component added later is reported without being named",
  "[contents]"
) {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  REQUIRE(!has(componentNames(world, entity), "RigidBody"));

  world.setRigidBody(entity, 0u);

  REQUIRE(has(componentNames(world, entity), "RigidBody"));
}

TEST_CASE("the reactive queues are not components", "[contents]") {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  const auto names = componentNames(world, entity);

  REQUIRE(!has(names, "reactive"));

  for (const auto& name : names) {
    REQUIRE(name.find("::") == std::string::npos);
  }
}

TEST_CASE("an entity that is not live reports nothing", "[contents]") {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  REQUIRE(world.destroy(entity));

  REQUIRE(componentNames(world, entity).empty());
}

TEST_CASE(
  "a runtime-declared component is listed like any other",
  "[worldcontents]"
) {
  World world;

  const Entity entity = world.createSdfGrid({}, {});

  const dunya::objectmodel::ComponentType type =
    world.dynamic().declare(dunya::objectmodel::ComponentSpec{"Wager", 4u, {}});

  REQUIRE(type != dunya::objectmodel::INVALID_COMPONENT_TYPE);

  const std::array<std::byte, 4> value{};

  REQUIRE(world.dynamic().emplace(type, entity, value));

  const std::vector<std::string> names =
    dunya::objectmodel::componentNames(world, entity);

  REQUIRE(std::find(names.begin(), names.end(), "Wager") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "Pose") != names.end());
}

TEST_CASE(
  "an entity without the declared component does not list it",
  "[worldcontents]"
) {
  World world;

  const Entity carrier = world.createSdfGrid({}, {});
  const Entity bare = world.createSdfGrid({}, {});

  const dunya::objectmodel::ComponentType type =
    world.dynamic().declare(dunya::objectmodel::ComponentSpec{"Wager", 4u, {}});

  const std::array<std::byte, 4> value{};

  REQUIRE(world.dynamic().emplace(type, carrier, value));

  const std::vector<std::string> names =
    dunya::objectmodel::componentNames(world, bare);

  REQUIRE(std::find(names.begin(), names.end(), "Wager") == names.end());
}
