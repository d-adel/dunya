#include <catch2/catch_test_macros.hpp>

#include <dunya/objectmodel/component/lens/lens.h>
#include <dunya/objectmodel/component/maincamera/maincamera.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/objectmodel/worldquery/worldquery.h>

#include <entt/entity/registry.hpp>

using dunya::objectmodel::Entity;
using dunya::objectmodel::Lens;
using dunya::objectmodel::MainCamera;
using dunya::objectmodel::Pose;
using dunya::objectmodel::World;

namespace {

Entity camera(World& world) {
  const Entity eye = world.createAuthored();

  world.emplaceAuthored<Pose>(eye, Pose{});
  world.emplaceAuthored<Lens>(eye, Lens{});

  return eye;
}

uint32_t tagged(const World& world) {
  return uint32_t(world.registry().view<const MainCamera>().size());
}

}

TEST_CASE("an untagged camera is not the main one", "[maincamera]") {
  World world;

  const Entity eye = camera(world);

  REQUIRE(tagged(world) == 0u);
  REQUIRE(
    dunya::objectmodel::mainCamera(world) == dunya::objectmodel::INVALID_ENTITY
  );

  REQUIRE(world.setMainCamera(eye));
  REQUIRE(dunya::objectmodel::mainCamera(world) == eye);
}

TEST_CASE("the tagged camera wins over the first one", "[maincamera]") {
  World world;

  const Entity first = camera(world);
  const Entity second = camera(world);

  REQUIRE(world.setMainCamera(first));
  REQUIRE(dunya::objectmodel::mainCamera(world) == first);
  REQUIRE(world.setMainCamera(second));
  REQUIRE(dunya::objectmodel::mainCamera(world) == second);
}

TEST_CASE("only one camera is ever the main one", "[maincamera]") {
  World world;

  const Entity first = camera(world);
  const Entity second = camera(world);

  REQUIRE(world.setMainCamera(first));
  REQUIRE(world.setMainCamera(second));

  REQUIRE(tagged(world) == 1u);
  REQUIRE(dunya::objectmodel::mainCamera(world) == second);
}

TEST_CASE(
  "an entity without a lens cannot be the main camera",
  "[maincamera]"
) {
  World world;

  const Entity bare = world.createAuthored();

  world.emplaceAuthored<Pose>(bare, Pose{});

  REQUIRE_FALSE(world.setMainCamera(bare));
  REQUIRE(tagged(world) == 0u);
}

TEST_CASE("a world with no camera at all reports none", "[maincamera]") {
  World world;

  REQUIRE(
    dunya::objectmodel::mainCamera(world) == dunya::objectmodel::INVALID_ENTITY
  );
}
