#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/core/config/config.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/material/material.h>
#include <dunya/objectmodel/mesh/mesh.h>
#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/rigidbody/rigidbody.h>
#include <dunya/objectmodel/selfcontained/selfcontained.h>
#include <dunya/objectmodel/staticbody/staticbody.h>
#include <dunya/objectmodel/world/world.h>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

#include "tolerances.h"

namespace {

using Catch::Matchers::WithinAbs;

using dunya::objectmodel::BakedVolume;
using dunya::objectmodel::Entity;
using dunya::objectmodel::Pose;
using dunya::objectmodel::SdfGrid;
using dunya::objectmodel::World;

dunya::field::Primitive marker(uint32_t material) {
  return dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, material);
}

SdfGrid blank() {
  SdfGrid grid{};

  grid.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  return grid;
}

const SdfGrid& gridOf(const World& world, Entity entity) {
  return world.registry().get<SdfGrid>(entity);
}

const Pose& poseOf(const World& world, Entity entity) {
  return world.registry().get<Pose>(entity);
}

uint32_t materialAt(const World& world, Entity entity, uint32_t index) {
  return world.primitives(entity)[index].shapeConfig.y;
}

uint32_t liveEntityCount(const World& world) {
  return static_cast<uint32_t>(world.registry().storage<Entity>()->free_list());
}

}

TEST_CASE("a created field is live and listed", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  REQUIRE(world.needsBake(entity));

  REQUIRE(world.registry().valid(entity));
  REQUIRE(world.registry().all_of<SdfGrid>(entity));

  REQUIRE(world.fields().size() == 1);
  REQUIRE(world.fields()[0] == entity);
}

TEST_CASE("the registry is reachable read-only", "[world]") {
  static_assert(
    std::is_same_v<
      decltype(std::declval<World&>().registry()),
      const entt::registry&>,
    "World::registry() must stay const-only"
  );

  SUCCEED("checked at compile time");
}

TEST_CASE("placing at a hint restores the exact identity", "[world]") {
  World world;

  Pose marked{};
  marked.position.x = 10.0f;

  const Entity entity = world.createField(marked, blank());

  REQUIRE(world.destroyField(entity));
  REQUIRE_FALSE(world.registry().valid(entity));

  marked.position.x = 11.0f;

  REQUIRE(world.createFieldAt(entity, marked, blank()));

  REQUIRE(world.registry().valid(entity));
  REQUIRE_THAT(
    poseOf(world, entity).position.x,
    WithinAbs(11.0f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("a taken hint is refused and leaves nothing behind", "[world]") {
  World world;

  const Entity first = world.createField(Pose{}, blank());

  REQUIRE(world.destroyField(first));

  const Entity recycled = world.createField(Pose{}, blank());

  const uint32_t before = liveEntityCount(world);

  REQUIRE_FALSE(world.createFieldAt(first, Pose{}, blank()));

  REQUIRE(world.fields().size() == 1);
  REQUIRE(world.fields()[0] == recycled);
  REQUIRE(liveEntityCount(world) == before);
}

TEST_CASE("destroying a field returns its primitives to the pool", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  REQUIRE(world.addPrimitive(entity, marker(1)));
  REQUIRE(world.addPrimitive(entity, marker(2)));
  REQUIRE(world.addPrimitive(entity, marker(3)));

  const size_t used = world.pool().size();

  REQUIRE(used > 0);

  REQUIRE(world.destroyField(entity));

  REQUIRE(world.pool().empty());

  const Entity next = world.createField(Pose{}, blank());

  REQUIRE(world.addPrimitive(next, marker(4)));
  REQUIRE(world.addPrimitive(next, marker(5)));
  REQUIRE(world.addPrimitive(next, marker(6)));

  REQUIRE(world.pool().size() == used);
}

TEST_CASE("destroying an entity that carries no field is refused", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  REQUIRE(world.destroyField(entity));

  REQUIRE_FALSE(world.destroyField(entity));
}

TEST_CASE(
  "the primitive transactions are visible through the world",
  "[world]"
) {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  REQUIRE(world.addPrimitive(entity, marker(1)));
  REQUIRE(world.addPrimitive(entity, marker(3)));
  REQUIRE(world.insertPrimitive(entity, 1, marker(2)));

  REQUIRE(world.primitiveCount(entity) == 3);
  REQUIRE(materialAt(world, entity, 0) == 1);
  REQUIRE(materialAt(world, entity, 1) == 2);
  REQUIRE(materialAt(world, entity, 2) == 3);

  REQUIRE(world.setPrimitive(entity, 1, marker(9)));
  REQUIRE(materialAt(world, entity, 1) == 9);

  REQUIRE(world.removePrimitive(entity, 0));
  REQUIRE(world.primitiveCount(entity) == 2);
  REQUIRE(materialAt(world, entity, 0) == 9);
  REQUIRE(materialAt(world, entity, 1) == 3);
}

TEST_CASE(
  "a primitive edit re-fits the grid and queues it for bake",
  "[world]"
) {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  world.markBaked(entity);

  REQUIRE_FALSE(world.needsBake(entity));

  REQUIRE(world.addPrimitive(entity, marker(1)));

  const SdfGrid& grid = gridOf(world, entity);

  REQUIRE(world.needsBake(entity));

  const float expected = -(1.0f + dunya::core::FIELD_GRID_MARGIN);

  REQUIRE_THAT(grid.origin.x, WithinAbs(expected, ANALYTIC_TOLERANCE));
  REQUIRE(grid.voxelSize.x > 0.0f);
}

TEST_CASE("replace writes the whole pose", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  const glm::vec3 position(1.0f, 2.0f, 3.0f);

  const glm::quat rotation =
    glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  world.replace<Pose>(entity, Pose{position, rotation});

  const Pose& pose = poseOf(world, entity);

  REQUIRE_THAT(pose.position.x, WithinAbs(1.0f, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(pose.position.z, WithinAbs(3.0f, ANALYTIC_TOLERANCE));

  REQUIRE_THAT(pose.rotation.w, WithinAbs(rotation.w, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(pose.rotation.y, WithinAbs(rotation.y, ANALYTIC_TOLERANCE));
}

TEST_CASE("the component setters reach the object", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  REQUIRE_FALSE(world.registry().all_of<BakedVolume>(entity));

  world.setBakedVolume(entity, 3);

  REQUIRE(world.registry().all_of<BakedVolume>(entity));
  REQUIRE(world.registry().get<BakedVolume>(entity).index == 3);
}

TEST_CASE(
  "setting a baked volume twice replaces rather than throws",
  "[world]"
) {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  world.setBakedVolume(entity, 3);
  world.setBakedVolume(entity, 7);

  REQUIRE(world.registry().get<BakedVolume>(entity).index == 7);
}

TEST_CASE("the field span follows creates and destroys", "[world]") {
  World world;

  const Entity first = world.createField(Pose{}, blank());
  const Entity second = world.createField(Pose{}, blank());
  const Entity third = world.createField(Pose{}, blank());

  REQUIRE(world.fields().size() == 3);

  REQUIRE(world.destroyField(second));

  const std::span<const Entity> remaining = world.fields();

  REQUIRE(remaining.size() == 2);

  REQUIRE(
    std::find(remaining.begin(), remaining.end(), second) == remaining.end()
  );
  REQUIRE(
    std::find(remaining.begin(), remaining.end(), first) != remaining.end()
  );
  REQUIRE(
    std::find(remaining.begin(), remaining.end(), third) != remaining.end()
  );
}

TEST_CASE(
  "only self-contained components reach the generic mutations",
  "[world]"
) {
  static_assert(dunya::objectmodel::SelfContained<Pose>);
  static_assert(dunya::objectmodel::SelfContained<dunya::objectmodel::Mesh>);
  static_assert(
    dunya::objectmodel::SelfContained<dunya::objectmodel::Material>
  );

  static_assert(!dunya::objectmodel::SelfContained<SdfGrid>);
  static_assert(!dunya::objectmodel::SelfContained<BakedVolume>);
  static_assert(
    !dunya::objectmodel::SelfContained<dunya::objectmodel::SdfPrimitiveRange>
  );

  static_assert(
    !dunya::objectmodel::SelfContained<dunya::objectmodel::RigidBody>
  );
  static_assert(
    !dunya::objectmodel::SelfContained<dunya::objectmodel::StaticBody>
  );

  SUCCEED("checked at compile time");
}

TEST_CASE("patch writes only what it touches", "[world]") {
  World world;

  const glm::quat rotation =
    glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  const Entity entity =
    world.createField(Pose{glm::vec3(0.0f), rotation}, blank());

  world.patch<Pose>(entity, [](Pose& pose) { pose.position.x = 5.0f; });

  const Pose& pose = poseOf(world, entity);

  REQUIRE_THAT(pose.position.x, WithinAbs(5.0f, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(pose.rotation.y, WithinAbs(rotation.y, ANALYTIC_TOLERANCE));
}

TEST_CASE(
  "the physics components keep the shape their use depends on",
  "[world]"
) {
  static_assert(std::is_empty_v<dunya::objectmodel::StaticBody>);

  REQUIRE(dunya::objectmodel::RigidBody{}.id == UINT32_MAX);
}

TEST_CASE("the CPU field is stored and read back", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  dunya::field::SampledField field;
  field.origin = glm::vec3(-1.0f, -2.0f, -3.0f);
  field.voxelSize = glm::vec3(0.25f);
  field.resolution = glm::uvec3(3u);
  field.distances.assign(27u, 0.5f);
  field.distances[13] = -0.75f;

  world.setSampledField(entity, std::move(field));

  REQUIRE(world.hasSampledField(entity));

  const dunya::field::SampledField& stored = *world.sampledField(entity);

  REQUIRE(stored.resolution == glm::uvec3(3u));
  REQUIRE(stored.distances.size() == 27u);
  REQUIRE_THAT(stored.distances[13], WithinAbs(-0.75f, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(stored.origin.y, WithinAbs(-2.0f, ANALYTIC_TOLERANCE));

  static_assert(
    !dunya::objectmodel::selfContained<dunya::field::SampledField>,
    "a sampled field is derived, so only the bake may write it"
  );
}

TEST_CASE("a stored field keeps its address when another goes", "[world]") {
  World world;

  const Entity first = world.createField(Pose{}, blank());
  const Entity second = world.createField(Pose{}, blank());

  dunya::field::SampledField a;
  a.resolution = glm::uvec3(2u);
  a.distances.assign(8u, 1.0f);

  dunya::field::SampledField b;
  b.resolution = glm::uvec3(2u);
  b.distances.assign(8u, -2.0f);

  world.setSampledField(first, std::move(a));
  world.setSampledField(second, std::move(b));

  const dunya::field::SampledField* held = world.sampledField(second);

  REQUIRE(world.destroyField(first));

  REQUIRE(held == world.sampledField(second));
  REQUIRE(held->distances.size() == 8u);
  REQUIRE_THAT(held->distances[0], WithinAbs(-2.0f, ANALYTIC_TOLERANCE));
}

TEST_CASE("a shared lattice is one lattice, not two", "[world]") {
  World world;

  const Entity donor = world.createField(Pose{}, blank());
  const Entity taker = world.createField(Pose{}, blank());

  dunya::field::SampledField field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledField(donor, std::move(field));

  REQUIRE(world.sampledFieldUsers(donor) == 1);

  world.shareSampledField(donor, taker);

  REQUIRE(world.sampledField(taker) == world.sampledField(donor));
  REQUIRE(world.sampledFieldUsers(donor) == 2);
  REQUIRE(world.sampledFieldUsers(taker) == 2);

  REQUIRE_FALSE(world.needsResample(taker));
}

TEST_CASE("a dent on a shared lattice takes a private copy", "[world]") {
  World world;

  const Entity donor = world.createField(Pose{}, blank());
  const Entity taker = world.createField(Pose{}, blank());

  for (const Entity entity : {donor, taker}) {
    world.emplaceOrReplace<dunya::objectmodel::Deformable>(
      entity,
      dunya::objectmodel::Deformable{}
    );
  }

  dunya::field::SampledField field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledField(donor, std::move(field));
  world.shareSampledField(donor, taker);

  const dunya::field::SampledField* before = world.sampledField(donor);

  world.patchSampledField(taker, [](dunya::field::SampledField& lattice) {
    lattice.distances[0] = -5.0f;
  });

  REQUIRE(world.sampledField(taker) != before);
  REQUIRE(world.sampledField(donor) == before);

  REQUIRE_THAT(
    world.sampledField(taker)->distances[0],
    WithinAbs(-5.0f, ANALYTIC_TOLERANCE)
  );

  REQUIRE_THAT(
    world.sampledField(donor)->distances[0],
    WithinAbs(1.0f, ANALYTIC_TOLERANCE)
  );

  REQUIRE(world.sampledFieldUsers(donor) == 1);
  REQUIRE(world.sampledFieldUsers(taker) == 1);
}

TEST_CASE("a dent on an unshared lattice copies nothing", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );

  dunya::field::SampledField field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledField(entity, std::move(field));

  const dunya::field::SampledField* before = world.sampledField(entity);

  world.patchSampledField(entity, [](dunya::field::SampledField& lattice) {
    lattice.distances[0] = -5.0f;
  });

  REQUIRE(world.sampledField(entity) == before);
}

TEST_CASE("a dent records that the lattice left its primitives", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );

  dunya::field::SampledField field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledField(entity, std::move(field));

  REQUIRE_FALSE(world.registry().all_of<dunya::objectmodel::Deformed>(entity));

  world.patchSampledField(entity, [](dunya::field::SampledField& lattice) {
    lattice.distances[0] = -1.0f;
  });

  REQUIRE(world.registry().all_of<dunya::objectmodel::Deformed>(entity));
}

TEST_CASE("a fresh bake puts the lattice back on its primitives", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );

  dunya::field::SampledField field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledField(entity, field);

  world.patchSampledField(entity, [](dunya::field::SampledField& lattice) {
    lattice.distances[0] = -1.0f;
  });

  REQUIRE(world.registry().all_of<dunya::objectmodel::Deformed>(entity));

  world.setSampledField(entity, field);

  REQUIRE_FALSE(world.registry().all_of<dunya::objectmodel::Deformed>(entity));
}
