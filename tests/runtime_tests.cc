#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fieldprimitives.h"

#include <dunya/core/config/config.h>
#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>
#include <dunya/objectmodel/massscale/massscale.h>
#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/rigidbody/rigidbody.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/staticbody/staticbody.h>
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

// Bakes what the entity's primitives currently say, at a box that fits them,
// which is what the frame loop does after an edit.
void rebake(World& world, Entity entity) {
  const dunya::objectmodel::SdfGrid& grid =
    world.registry().get<dunya::objectmodel::SdfGrid>(entity);

  const dunya::field::Aabb box =
    dunya::objectmodel::gridBox(world.primitives(entity));

  world.setSampledField(
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

  const Entity entity = world.createField(dunya::objectmodel::Pose{}, grid);

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

}  // namespace

TEST_CASE("a mass override survives a rebake as a density", "[runtime]") {
  // SetShape recomputes mass from the new geometry - which is what makes a
  // carved body lighter, and the reason to keep geometry as a volume at all -
  // and in doing so discards whatever setMass asked for. What is remembered is
  // the factor, so both hold at once: the ball stays as dense as it was made,
  // and losing seven eighths of itself costs it seven eighths of its weight.
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

  // Half the radius is an eighth of the volume, through the edit path: the
  // primitive shrinks, the field is rebaked from it, the shape is rebuilt.
  REQUIRE(
    live.setPrimitive(entity, 0u, fixture::sphere(glm::vec3(0.0f), 0.5f))
  );
  rebake(live, entity);
  runtime.refreshBody(entity);

  REQUIRE_THAT(bodyMass(runtime, entity), WithinRel(150.0f / 8.0f, 0.1f));
}

TEST_CASE("a body nobody weighed follows its geometry", "[runtime]") {
  // The other half of the same property, and the one that must not regress:
  // absent a MassScale nothing is put back, so the shape's own mass stands.
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

// A field sphere and a static field slab in one authored world, which is what
// the criterion is actually about: the trajectory has to be reproducible over
// this project's collider, not over one Jolt ships.
void fillFieldWorld(dunya::objectmodel::World& world) {
  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(33u);

  dunya::objectmodel::Pose floorPose{};
  floorPose.position = glm::vec3(0.0f, -1.0f, 0.0f);

  const dunya::objectmodel::Entity floor = world.createField(floorPose, grid);

  REQUIRE(world.addPrimitive(
    floor,
    fixture::box(glm::vec3(0.0f), glm::vec3(4.0f, 0.5f, 4.0f))
  ));
  world.addStaticBody(floor);

  dunya::objectmodel::Pose ballPose{};
  ballPose.position = glm::vec3(0.13f, 1.7f, 0.07f);

  const dunya::objectmodel::Entity ball = world.createField(ballPose, grid);

  REQUIRE(world.addPrimitive(ball, fixture::sphere(glm::vec3(0.0f), 0.4f)));

  for (const dunya::objectmodel::Entity entity : world.fields()) {
    const dunya::objectmodel::SdfGrid& fitted =
      world.registry().get<dunya::objectmodel::SdfGrid>(entity);
    const dunya::field::Aabb box =
      dunya::objectmodel::gridBox(world.primitives(entity));

    world.setSampledField(
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

  for (const dunya::objectmodel::Entity entity : runtime.world().fields()) {
    runtime.refreshBody(entity);
  }

  std::vector<float> heights;

  for (uint32_t step = 0u; step != steps; ++step) {
    runtime.step();
    runtime.syncPoses();

    for (const dunya::objectmodel::Entity entity : runtime.world().fields()) {
      heights.push_back(runtime.world()
                          .registry()
                          .get<dunya::objectmodel::Pose>(entity)
                          .position.y);
    }
  }

  return heights;
}

}  // namespace

TEST_CASE(
  "the same initial conditions reproduce the same trajectory over a field",
  "[physics]"
) {
  // M18's criterion, over this project's collider rather than over Jolt's own
  // shapes. Contacts here come from a seed walk on worker threads, so nothing
  // about the shipped test above carries: it proves Jolt is deterministic, not
  // that FieldShape is.
  JoltLibrary library;

  dunya::objectmodel::World source;
  fillFieldWorld(source);

  const std::vector<float> first = fieldDrop(source, library, 120u);
  const std::vector<float> second = fieldDrop(source, library, 120u);

  REQUIRE(first.size() == second.size());
  REQUIRE(first.size() == 240u);

  // Bit-identical, not approximately equal: a tolerance would hide exactly the
  // drift the criterion is about.
  for (size_t i = 0; i != first.size(); ++i) {
    INFO("sample " << i);
    REQUIRE(first[i] == second[i]);
  }

  // And it has to have actually fallen and stopped, or the comparison is
  // between two rows of the same starting number.
  REQUIRE(first.back() < 1.0f);
  REQUIRE(first.back() > -1.0f);
}

TEST_CASE("a field with nothing solid in it gets no body", "[runtime]") {
  // Jolt asserts on a zero mass, so this used to take the process with it.
  // Carving an object away is a legitimate thing to do to a volume, and a
  // milestone away from being the normal thing, so it cannot.
  JoltLibrary library;

  World source;

  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(17u);

  const Entity entity = source.createField(dunya::objectmodel::Pose{}, grid);

  // Baked around a box the sphere is nowhere near, which is the state a carve
  // that removes everything leaves behind.
  source.setSampledField(
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
