#include <catch2/catch_test_macros.hpp>

#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/physics/layers/layers.h>
#include <dunya/physics/physicsworld/physicsworld.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/runtime/runtime/runtime.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <vector>

using dunya::physics::JoltLibrary;
using dunya::physics::PhysicsWorld;

TEST_CASE("the jolt library registers and unregisters", "[physics]") {
  JoltLibrary library;

  SUCCEED();
}

TEST_CASE("a box shape can be created after registration", "[physics]") {
  JoltLibrary library;

  JPH::BoxShapeSettings settings(JPH::Vec3(10.0f, 0.5f, 10.0f));
  settings.SetEmbedded();

  const JPH::ShapeSettings::ShapeResult result = settings.Create();

  REQUIRE_FALSE(result.HasError());
}

TEST_CASE("a physics world initialises", "[physics]") {
  JoltLibrary library;
  PhysicsWorld world;

  SUCCEED();
}

TEST_CASE("a sphere falls onto a floor and comes to rest", "[physics]") {
  JoltLibrary library;
  PhysicsWorld world;

  JPH::BodyInterface& bodies = world.bodies();

  JPH::BoxShapeSettings floorSettings(JPH::Vec3(10.0f, 0.5f, 10.0f));
  floorSettings.SetEmbedded();

  const JPH::ShapeSettings::ShapeResult floorShape = floorSettings.Create();
  REQUIRE_FALSE(floorShape.HasError());

  const JPH::BodyID floorId = bodies.CreateAndAddBody(
    JPH::BodyCreationSettings(
      floorShape.Get(),
      JPH::RVec3(0.0f, -0.5f, 0.0f),
      JPH::Quat::sIdentity(),
      JPH::EMotionType::Static,
      dunya::physics::ObjectLayers::NON_MOVING
    ),
    JPH::EActivation::DontActivate
  );
  REQUIRE_FALSE(floorId.IsInvalid());

  const JPH::BodyID sphereId = bodies.CreateAndAddBody(
    JPH::BodyCreationSettings(
      new JPH::SphereShape(0.5f),
      JPH::RVec3(0.0f, 5.0f, 0.0f),
      JPH::Quat::sIdentity(),
      JPH::EMotionType::Dynamic,
      dunya::physics::ObjectLayers::MOVING
    ),
    JPH::EActivation::Activate
  );
  REQUIRE_FALSE(sphereId.IsInvalid());

  world.optimizeBroadPhase();

  for (int i = 0; i != 120; ++i) {
    world.step();
  }

  const float restingHeight = bodies.GetCenterOfMassPosition(sphereId).GetY();

  REQUIRE(restingHeight > 0.4f);
  REQUIRE(restingHeight < 0.6f);

  bodies.RemoveBody(sphereId);
  bodies.DestroyBody(sphereId);
  bodies.RemoveBody(floorId);
  bodies.DestroyBody(floorId);
}

namespace {

std::vector<float> dropTrajectory(int steps) {
  JoltLibrary library;
  PhysicsWorld world;

  JPH::BodyInterface& bodies = world.bodies();

  JPH::BoxShapeSettings floorSettings(JPH::Vec3(10.0f, 0.5f, 10.0f));
  floorSettings.SetEmbedded();

  const JPH::BodyID floorId = bodies.CreateAndAddBody(
    JPH::BodyCreationSettings(
      floorSettings.Create().Get(),
      JPH::RVec3(0.0f, -0.5f, 0.0f),
      JPH::Quat::sIdentity(),
      JPH::EMotionType::Static,
      dunya::physics::ObjectLayers::NON_MOVING
    ),
    JPH::EActivation::DontActivate
  );

  const JPH::BodyID sphereId = bodies.CreateAndAddBody(
    JPH::BodyCreationSettings(
      new JPH::SphereShape(0.5f),
      JPH::RVec3(0.0f, 5.0f, 0.0f),
      JPH::Quat::sIdentity(),
      JPH::EMotionType::Dynamic,
      dunya::physics::ObjectLayers::MOVING
    ),
    JPH::EActivation::Activate
  );

  world.optimizeBroadPhase();

  std::vector<float> heights;
  heights.reserve(static_cast<size_t>(steps));

  for (int i = 0; i != steps; ++i) {
    world.step();
    heights.push_back(bodies.GetCenterOfMassPosition(sphereId).GetY());
  }

  bodies.RemoveBody(sphereId);
  bodies.DestroyBody(sphereId);
  bodies.RemoveBody(floorId);
  bodies.DestroyBody(floorId);

  return heights;
}

}

TEST_CASE(
  "the same initial conditions reproduce the same trajectory",
  "[physics]"
) {
  const std::vector<float> first = dropTrajectory(120);
  const std::vector<float> second = dropTrajectory(120);

  REQUIRE(first.size() == second.size());

  for (size_t i = 0; i != first.size(); ++i) {
    INFO("step " << i);
    REQUIRE(first[i] == second[i]);
  }
}

TEST_CASE("a runtime can be constructed", "[runtime]") {
  JoltLibrary library;
  dunya::objectmodel::World source;

  dunya::runtime::Runtime runtime(source, library);

  SUCCEED();
}
