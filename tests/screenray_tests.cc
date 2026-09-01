#include <catch2/catch_test_macros.hpp>

#include <dunya/objectmodel/component/lens/lens.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/objectmodel/worldquery/worldquery.h>

#include <cmath>

using dunya::objectmodel::Entity;
using dunya::objectmodel::Lens;
using dunya::objectmodel::Pose;
using dunya::objectmodel::World;

namespace {

constexpr glm::vec2 VIEWPORT{1280.0f, 720.0f};

Entity camera(World& world, const glm::vec3& at = glm::vec3(0.0f)) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(9u);

  Pose pose{};
  pose.position = at;

  const Entity eye = world.createSdfGrid(pose, grid);

  world.emplaceOrReplace<Lens>(eye, Lens{});

  return eye;
}

}

TEST_CASE("the centre of the screen looks straight ahead", "[screenray]") {
  World world;

  const Entity eye = camera(world, glm::vec3(0.0f, 0.0f, 5.0f));

  const std::optional<dunya::field::Ray> ray =
    dunya::objectmodel::screenPointToRay(world, eye, VIEWPORT * 0.5f, VIEWPORT);

  REQUIRE(ray.has_value());

  REQUIRE(std::abs(ray->direction.x) < 1.0e-4f);
  REQUIRE(std::abs(ray->direction.y) < 1.0e-4f);
  REQUIRE(ray->direction.z < -0.99f);
}

TEST_CASE("the aim follows the cursor across the screen", "[screenray]") {
  World world;

  const Entity eye = camera(world, glm::vec3(0.0f, 0.0f, 5.0f));

  const std::optional<dunya::field::Ray> left =
    dunya::objectmodel::screenPointToRay(
      world,
      eye,
      glm::vec2(VIEWPORT.x * 0.25f, VIEWPORT.y * 0.5f),
      VIEWPORT
    );

  const std::optional<dunya::field::Ray> right =
    dunya::objectmodel::screenPointToRay(
      world,
      eye,
      glm::vec2(VIEWPORT.x * 0.75f, VIEWPORT.y * 0.5f),
      VIEWPORT
    );

  const std::optional<dunya::field::Ray> up =
    dunya::objectmodel::screenPointToRay(
      world,
      eye,
      glm::vec2(VIEWPORT.x * 0.5f, VIEWPORT.y * 0.25f),
      VIEWPORT
    );

  REQUIRE(left.has_value());
  REQUIRE(right.has_value());
  REQUIRE(up.has_value());

  REQUIRE(left->direction.x < -0.1f);
  REQUIRE(right->direction.x > 0.1f);
  REQUIRE(up->direction.y > 0.1f);

  REQUIRE(left->direction.x < right->direction.x);
}

TEST_CASE("the ray leaves the camera it was asked about", "[screenray]") {
  World world;

  const glm::vec3 seat(2.0f, 3.0f, 4.0f);

  const Entity eye = camera(world, seat);

  const std::optional<dunya::field::Ray> ray =
    dunya::objectmodel::screenPointToRay(world, eye, VIEWPORT * 0.5f, VIEWPORT);

  REQUIRE(ray.has_value());
  REQUIRE(glm::length(ray->origin - seat) < 1.0e-3f);
}

TEST_CASE("a camera turned about Y aims where it faces", "[screenray]") {
  World world;

  const Entity eye = camera(world);

  world.patch<Pose>(eye, [](Pose& pose) {
    pose.rotation =
      glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  });

  const std::optional<dunya::field::Ray> ray =
    dunya::objectmodel::screenPointToRay(world, eye, VIEWPORT * 0.5f, VIEWPORT);

  REQUIRE(ray.has_value());
  REQUIRE(ray->direction.x < -0.99f);
}

TEST_CASE("an entity with no lens gives no ray", "[screenray]") {
  World world;

  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(9u);

  const Entity blind = world.createSdfGrid(Pose{}, grid);

  REQUIRE_FALSE(
    dunya::objectmodel::screenPointToRay(
      world,
      blind,
      VIEWPORT * 0.5f,
      VIEWPORT
    )
      .has_value()
  );
}

TEST_CASE("a viewport with no area gives no ray", "[screenray]") {
  World world;

  const Entity eye = camera(world);

  REQUIRE_FALSE(
    dunya::objectmodel::screenPointToRay(
      world,
      eye,
      glm::vec2(0.0f),
      glm::vec2(0.0f)
    )
      .has_value()
  );
}
