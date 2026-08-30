#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fieldprimitives.h"

#include <dunya/core/config/config.h>
#include <dunya/field/field.h>
#include <dunya/field/sampledsdf/sampledsdf.h>
#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/massscale/massscale.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/rigidbody/rigidbody.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/component/staticbody/staticbody.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/physics/joltlibrary/joltlibrary.h>
#include <dunya/runtime/runtime/runtime.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionProperties.h>

#include <glm/glm.hpp>

#include <vector>

using Catch::Matchers::WithinRel;
using dunya::objectmodel::Entity;
using dunya::objectmodel::World;
using dunya::physics::JoltLibrary;
using dunya::runtime::Runtime;

namespace {

constexpr uint32_t RESOLUTION = 33u;

void rebake(World& world, Entity entity) {
  const dunya::objectmodel::SdfGrid& grid =
    world.registry().get<dunya::objectmodel::SdfGrid>(entity);

  const dunya::field::Aabb box =
    dunya::objectmodel::gridBox(world.primitives(entity));

  world.setSampledSdf(
    entity,
    dunya::field::bake(
      world.primitives(entity),
      box.minimum,
      box.maximum,
      grid.resolution
    )
  );
}

Entity sphereEntity(World& world, float radius) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(RESOLUTION);

  const Entity entity = world.createSdfGrid(dunya::objectmodel::Pose{}, grid);

  REQUIRE(world.addPrimitive(entity, fixture::sphere(glm::vec3(0.0f), radius)));
  rebake(world, entity);

  return entity;
}

float bodyMass(Runtime& runtime, Entity entity) {
  const auto& body =
    runtime.world().registry().get<dunya::objectmodel::RigidBody>(entity);

  JPH::BodyLockRead lock(
    runtime.physics().system().GetBodyLockInterface(),
    JPH::BodyID(body.id)
  );

  REQUIRE(lock.Succeeded());

  return 1.0f / lock.GetBody().GetMotionProperties()->GetInverseMass();
}

}

TEST_CASE("a mass override survives a rebake as a density", "[runtime]") {
  JoltLibrary library;

  World source;
  const Entity entity = sphereEntity(source, 1.0f);

  Runtime runtime(source, library);
  World& live = runtime.world();

  runtime.refreshBody(entity);

  const float derived = bodyMass(runtime, entity);

  REQUIRE(derived > 0.0f);

  runtime.setMass(entity, 150.0f);

  REQUIRE_THAT(bodyMass(runtime, entity), WithinRel(150.0f, 0.001f));

  REQUIRE(
    live.setPrimitive(entity, 0u, fixture::sphere(glm::vec3(0.0f), 0.5f))
  );
  rebake(live, entity);
  runtime.refreshBody(entity);

  REQUIRE_THAT(bodyMass(runtime, entity), WithinRel(150.0f / 8.0f, 0.1f));
}

TEST_CASE("a body nobody weighed follows its geometry", "[runtime]") {
  JoltLibrary library;

  World source;
  const Entity entity = sphereEntity(source, 1.0f);

  Runtime runtime(source, library);
  World& live = runtime.world();

  runtime.refreshBody(entity);

  const float derived = bodyMass(runtime, entity);

  REQUIRE_FALSE(live.registry().all_of<dunya::objectmodel::MassScale>(entity));

  REQUIRE(
    live.setPrimitive(entity, 0u, fixture::sphere(glm::vec3(0.0f), 0.5f))
  );
  rebake(live, entity);
  runtime.refreshBody(entity);

  REQUIRE_THAT(bodyMass(runtime, entity), WithinRel(derived / 8.0f, 0.1f));
}

namespace {

void fillFieldWorld(dunya::objectmodel::World& world) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(33u);

  dunya::objectmodel::Pose floorPose{};
  floorPose.position = glm::vec3(0.0f, -1.0f, 0.0f);

  const dunya::objectmodel::Entity floor = world.createSdfGrid(floorPose, grid);

  REQUIRE(world.addPrimitive(
    floor,
    fixture::box(glm::vec3(0.0f), glm::vec3(4.0f, 0.5f, 4.0f))
  ));
  world.addStaticBody(floor);

  dunya::objectmodel::Pose ballPose{};
  ballPose.position = glm::vec3(0.13f, 1.7f, 0.07f);

  const dunya::objectmodel::Entity ball = world.createSdfGrid(ballPose, grid);

  REQUIRE(world.addPrimitive(ball, fixture::sphere(glm::vec3(0.0f), 0.4f)));

  for (const dunya::objectmodel::Entity entity : world.sdfGrids()) {
    const dunya::objectmodel::SdfGrid& fitted =
      world.registry().get<dunya::objectmodel::SdfGrid>(entity);
    const dunya::field::Aabb box =
      dunya::objectmodel::gridBox(world.primitives(entity));

    world.setSampledSdf(
      entity,
      dunya::field::bake(
        world.primitives(entity),
        box.minimum,
        box.maximum,
        fitted.resolution
      )
    );
  }
}

std::vector<float> fieldDrop(
  const dunya::objectmodel::World& source,
  JoltLibrary& library,
  uint32_t steps
) {
  dunya::runtime::Runtime runtime(source, library);

  for (const dunya::objectmodel::Entity entity : runtime.world().sdfGrids()) {
    runtime.refreshBody(entity);
  }

  std::vector<float> heights;

  for (uint32_t step = 0u; step != steps; ++step) {
    runtime.step();
    runtime.syncPoses(1.0f);

    for (const dunya::objectmodel::Entity entity : runtime.world().sdfGrids()) {
      heights.push_back(runtime.world()
                          .registry()
                          .get<dunya::objectmodel::Pose>(entity)
                          .position.y);
    }
  }

  return heights;
}

}

TEST_CASE(
  "the same initial conditions reproduce the same trajectory over a field",
  "[physics]"
) {
  JoltLibrary library;

  dunya::objectmodel::World source;
  fillFieldWorld(source);

  const std::vector<float> first = fieldDrop(source, library, 120u);
  const std::vector<float> second = fieldDrop(source, library, 120u);

  REQUIRE(first.size() == second.size());
  REQUIRE(first.size() == 240u);

  for (size_t i = 0; i != first.size(); ++i) {
    INFO("sample " << i);
    REQUIRE(first[i] == second[i]);
  }

  REQUIRE(first.back() < 1.0f);
  REQUIRE(first.back() > -1.0f);
}

TEST_CASE("a field with nothing solid in it gets no body", "[runtime]") {
  JoltLibrary library;

  World source;

  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(17u);

  const Entity entity = source.createSdfGrid(dunya::objectmodel::Pose{}, grid);

  source.setSampledSdf(
    entity,
    dunya::field::bake(
      std::vector<dunya::field::Primitive>{
        fixture::sphere(glm::vec3(10.0f), 0.5f)
      },
      glm::vec3(-1.0f),
      glm::vec3(1.0f),
      grid.resolution
    )
  );

  Runtime runtime(source, library);

  runtime.refreshBody(entity);

  REQUIRE_FALSE(
    runtime.world().registry().all_of<dunya::objectmodel::RigidBody>(entity)
  );
}

namespace {

const JPH::Shape* bodyShape(Runtime& runtime, Entity entity) {
  const auto& body =
    runtime.world().registry().get<dunya::objectmodel::RigidBody>(entity);

  return runtime.physics().bodies().GetShape(JPH::BodyID(body.id));
}

}

TEST_CASE("objects on one lattice get one collision shape", "[runtime]") {
  JoltLibrary library;

  World source;

  const Entity first = sphereEntity(source, 1.0f);
  const Entity second = sphereEntity(source, 1.0f);

  source.shareSampledSdf(first, second);

  Runtime runtime(source, library);
  World& live = runtime.world();

  REQUIRE(live.sampledSdf(first) == live.sampledSdf(second));

  runtime.refreshBody(first);
  runtime.refreshBody(second);

  REQUIRE(bodyShape(runtime, first) == bodyShape(runtime, second));
  REQUIRE(runtime.shapeCount() == 1u);
}

TEST_CASE("a dent takes the object off the shared shape", "[runtime]") {
  JoltLibrary library;

  World source;

  const Entity first = sphereEntity(source, 1.0f);
  const Entity second = sphereEntity(source, 1.0f);

  source.shareSampledSdf(first, second);

  for (const Entity entity : {first, second}) {
    source.emplaceOrReplace<dunya::objectmodel::Deformable>(
      entity,
      dunya::objectmodel::Deformable{}
    );
  }

  Runtime runtime(source, library);
  World& live = runtime.world();

  runtime.refreshBody(first);
  runtime.refreshBody(second);

  REQUIRE(bodyShape(runtime, first) == bodyShape(runtime, second));

  live.patchSampledSdf(second, [](dunya::field::SampledSdf& lattice) {
    lattice.distances[0] = -1.0f;
  });

  REQUIRE(live.sampledSdf(first) != live.sampledSdf(second));

  runtime.refreshBody(second);

  REQUIRE(bodyShape(runtime, first) != bodyShape(runtime, second));
  REQUIRE(runtime.shapeCount() == 2u);
}

TEST_CASE("a rebuilt shape replaces the one the cache hands out", "[runtime]") {
  JoltLibrary library;

  World source;
  const Entity entity = sphereEntity(source, 1.0f);

  source.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );

  Runtime runtime(source, library);
  World& live = runtime.world();

  runtime.refreshBody(entity);

  REQUIRE(live.sampledSdfUsers(entity) == 2);

  live.patchSampledSdf(entity, [](dunya::field::SampledSdf& grid) {
    grid.distances[0] = -1.0f;
  });

  runtime.refreshBody(entity);

  const dunya::field::SampledSdf* lattice = live.sampledSdf(entity);

  REQUIRE(live.sampledSdfUsers(entity) == 1);

  live.patchSampledSdf(entity, [](dunya::field::SampledSdf& grid) {
    grid.distances[1] = -1.0f;
  });

  REQUIRE(live.sampledSdf(entity) == lattice);

  const glm::uvec3 bricks = dunya::field::brickCounts(*lattice);

  runtime.reshapeAfterDeform(entity, glm::uvec3(0u), bricks);

  const JPH::Shape* dented = bodyShape(runtime, entity);

  runtime.refreshBody(entity);

  REQUIRE(bodyShape(runtime, entity) == dented);
}
