#include <catch2/catch_test_macros.hpp>

#include <dunya/physics/impact/impact.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/physics/layers/layers.h>
#include <dunya/physics/physicsworld/physicsworld.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>
#include <vector>

using dunya::physics::Impact;
using dunya::physics::JoltLibrary;
using dunya::physics::PhysicsWorld;

namespace {

struct Drop {
  JPH::BodyID floor;
  JPH::BodyID sphere;
  std::vector<Impact> impacts;
};

void addFloor(PhysicsWorld& world, Drop& out) {
  JPH::BoxShapeSettings settings(JPH::Vec3(10.0f, 0.5f, 10.0f));
  settings.SetEmbedded();

  const JPH::ShapeSettings::ShapeResult shape = settings.Create();
  REQUIRE_FALSE(shape.HasError());

  out.floor = world.bodies().CreateAndAddBody(
    JPH::BodyCreationSettings(
      shape.Get(),
      JPH::RVec3(0.0f, -0.5f, 0.0f),
      JPH::Quat::sIdentity(),
      JPH::EMotionType::Static,
      dunya::physics::ObjectLayers::NON_MOVING
    ),
    JPH::EActivation::DontActivate
  );

  REQUIRE_FALSE(out.floor.IsInvalid());
}

void run(PhysicsWorld& world, Drop& out, float height, float speed, int steps) {
  JPH::BodyCreationSettings settings(
    new JPH::SphereShape(0.5f),
    JPH::RVec3(0.0f, height, 0.0f),
    JPH::Quat::sIdentity(),
    JPH::EMotionType::Dynamic,
    dunya::physics::ObjectLayers::MOVING
  );

  settings.mLinearVelocity = JPH::Vec3(0.0f, -speed, 0.0f);

  out.sphere =
    world.bodies().CreateAndAddBody(settings, JPH::EActivation::Activate);

  REQUIRE_FALSE(out.sphere.IsInvalid());

  world.optimizeBroadPhase();

  std::vector<Impact> batch;

  for (int i = 0; i != steps; ++i) {
    world.step();

    world.impacts().drain(batch);
    out.impacts.insert(out.impacts.end(), batch.begin(), batch.end());
  }
}

void destroy(PhysicsWorld& world, const Drop& drop) {
  JPH::BodyInterface& bodies = world.bodies();

  bodies.RemoveBody(drop.sphere);
  bodies.DestroyBody(drop.sphere);
  bodies.RemoveBody(drop.floor);
  bodies.DestroyBody(drop.floor);
}

}  // namespace

TEST_CASE("a hard landing is recorded for both bodies", "[impact]") {
  JoltLibrary library;
  PhysicsWorld world;

  Drop drop;
  addFloor(world, drop);
  run(world, drop, 3.0f, 15.0f, 60);

  REQUIRE_FALSE(drop.impacts.empty());

  REQUIRE(drop.impacts.size() % 2u == 0u);

  const Impact& first = drop.impacts[0];
  const Impact& second = drop.impacts[1];

  REQUIRE(first.speed > 10.0f);
  REQUIRE(second.speed == first.speed);
  REQUIRE(first.impulse > 0.0f);

  REQUIRE(std::abs(first.outward.y + second.outward.y) < 1e-5f);
  REQUIRE(std::abs(first.outward.y) > 0.9f);

  REQUIRE(std::abs(first.point.y) < 0.1f);

  destroy(world, drop);
}

TEST_CASE("a body settling under gravity records nothing", "[impact]") {
  JoltLibrary library;
  PhysicsWorld world;

  Drop drop;
  addFloor(world, drop);

  run(world, drop, 0.505f, 0.0f, 120);

  REQUIRE(drop.impacts.empty());

  const float resting =
    world.bodies().GetCenterOfMassPosition(drop.sphere).GetY();

  REQUIRE(resting > 0.4f);
  REQUIRE(resting < 0.6f);

  destroy(world, drop);
}

TEST_CASE("the threshold is what decides, and it moves", "[impact]") {
  JoltLibrary library;
  PhysicsWorld world;

  world.impacts().setThreshold(500.0f);

  REQUIRE(world.impacts().threshold() == 500.0f);

  Drop drop;
  addFloor(world, drop);
  run(world, drop, 3.0f, 15.0f, 60);

  REQUIRE(drop.impacts.empty());

  destroy(world, drop);
}

TEST_CASE("a drained listener does not repeat itself", "[impact]") {
  JoltLibrary library;
  PhysicsWorld world;

  Drop drop;
  addFloor(world, drop);
  run(world, drop, 3.0f, 15.0f, 60);

  REQUIRE_FALSE(drop.impacts.empty());

  std::vector<Impact> again;
  world.impacts().drain(again);

  REQUIRE(again.empty());

  destroy(world, drop);
}
