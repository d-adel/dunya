#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/core/config/config.h>
#include <dunya/objectmodel/component/directionallight/directionallight.h>
#include <dunya/objectmodel/component/environment/environment.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/component/lens/lens.h>
#include <dunya/objectmodel/component/material/material.h>
#include <dunya/objectmodel/component/mesh/mesh.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/rigidbody/rigidbody.h>
#include <dunya/objectmodel/trait/selfcontained/selfcontained.h>
#include <dunya/objectmodel/component/staticbody/staticbody.h>
#include <dunya/objectmodel/world/world.h>
#include <dunya/objectmodel/worldquery/worldquery.h>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

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

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  REQUIRE(world.needsBake(entity));

  REQUIRE(world.registry().valid(entity));
  REQUIRE(world.registry().all_of<SdfGrid>(entity));

  REQUIRE(world.sdfGrids().size() == 1);
  REQUIRE(world.sdfGrids()[0] == entity);
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

  const Entity entity = world.createSdfGrid(marked, blank());

  REQUIRE(world.destroy(entity));
  REQUIRE_FALSE(world.registry().valid(entity));

  marked.position.x = 11.0f;

  REQUIRE(world.createSdfGridAt(entity, marked, blank()));

  REQUIRE(world.registry().valid(entity));
  REQUIRE_THAT(
    poseOf(world, entity).position.x,
    WithinAbs(11.0f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("a taken hint is refused and leaves nothing behind", "[world]") {
  World world;

  const Entity first = world.createSdfGrid(Pose{}, blank());

  REQUIRE(world.destroy(first));

  const Entity recycled = world.createSdfGrid(Pose{}, blank());

  const uint32_t before = liveEntityCount(world);

  REQUIRE_FALSE(world.createSdfGridAt(first, Pose{}, blank()));

  REQUIRE(world.sdfGrids().size() == 1);
  REQUIRE(world.sdfGrids()[0] == recycled);
  REQUIRE(liveEntityCount(world) == before);
}

TEST_CASE("destroying a field returns its primitives to the pool", "[world]") {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  REQUIRE(world.addPrimitive(entity, marker(1)));
  REQUIRE(world.addPrimitive(entity, marker(2)));
  REQUIRE(world.addPrimitive(entity, marker(3)));

  const size_t used = world.pool().size();

  REQUIRE(used > 0);

  REQUIRE(world.destroy(entity));

  REQUIRE(world.pool().empty());

  const Entity next = world.createSdfGrid(Pose{}, blank());

  REQUIRE(world.addPrimitive(next, marker(4)));
  REQUIRE(world.addPrimitive(next, marker(5)));
  REQUIRE(world.addPrimitive(next, marker(6)));

  REQUIRE(world.pool().size() == used);
}

TEST_CASE("destroying an entity twice is refused", "[world]") {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  REQUIRE(world.destroy(entity));

  REQUIRE_FALSE(world.destroy(entity));
}

TEST_CASE("an entity that carries no field is destroyed", "[world]") {
  World world;

  const Entity entity = world.createAuthored();

  world.emplaceAuthored<Pose>(entity, Pose{});
  world.emplaceAuthored<dunya::objectmodel::Lens>(
    entity,
    dunya::objectmodel::Lens{}
  );

  REQUIRE(world.destroy(entity));

  REQUIRE_FALSE(world.registry().valid(entity));
}

TEST_CASE(
  "the primitive transactions are visible through the world",
  "[world]"
) {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

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

  const Entity entity = world.createSdfGrid(Pose{}, blank());

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

  const Entity entity = world.createSdfGrid(Pose{}, blank());

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

  const Entity entity = world.createSdfGrid(Pose{}, blank());

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

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  world.setBakedVolume(entity, 3);
  world.setBakedVolume(entity, 7);

  REQUIRE(world.registry().get<BakedVolume>(entity).index == 7);
}

TEST_CASE("the field span follows creates and destroys", "[world]") {
  World world;

  const Entity first = world.createSdfGrid(Pose{}, blank());
  const Entity second = world.createSdfGrid(Pose{}, blank());
  const Entity third = world.createSdfGrid(Pose{}, blank());

  REQUIRE(world.sdfGrids().size() == 3);

  REQUIRE(world.destroy(second));

  const std::span<const Entity> remaining = world.sdfGrids();

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
    world.createSdfGrid(Pose{glm::vec3(0.0f), rotation}, blank());

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

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  dunya::field::SampledSdf field;
  field.origin = glm::vec3(-1.0f, -2.0f, -3.0f);
  field.voxelSize = glm::vec3(0.25f);
  field.resolution = glm::uvec3(3u);
  field.distances.assign(27u, 0.5f);
  field.distances[13] = -0.75f;

  world.setSampledSdf(entity, std::move(field));

  REQUIRE(world.hasSampledSdf(entity));

  const dunya::field::SampledSdf& stored = *world.sampledSdf(entity);

  REQUIRE(stored.resolution == glm::uvec3(3u));
  REQUIRE(stored.distances.size() == 27u);
  REQUIRE_THAT(stored.distances[13], WithinAbs(-0.75f, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(stored.origin.y, WithinAbs(-2.0f, ANALYTIC_TOLERANCE));

  static_assert(
    !dunya::objectmodel::selfContained<dunya::field::SampledSdf>,
    "a sampled field is derived, so only the bake may write it"
  );
}

TEST_CASE("a stored field keeps its address when another goes", "[world]") {
  World world;

  const Entity first = world.createSdfGrid(Pose{}, blank());
  const Entity second = world.createSdfGrid(Pose{}, blank());

  dunya::field::SampledSdf a;
  a.resolution = glm::uvec3(2u);
  a.distances.assign(8u, 1.0f);

  dunya::field::SampledSdf b;
  b.resolution = glm::uvec3(2u);
  b.distances.assign(8u, -2.0f);

  world.setSampledSdf(first, std::move(a));
  world.setSampledSdf(second, std::move(b));

  const dunya::field::SampledSdf* held = world.sampledSdf(second);

  REQUIRE(world.destroy(first));

  REQUIRE(held == world.sampledSdf(second));
  REQUIRE(held->distances.size() == 8u);
  REQUIRE_THAT(held->distances[0], WithinAbs(-2.0f, ANALYTIC_TOLERANCE));
}

TEST_CASE("a shared lattice is one lattice, not two", "[world]") {
  World world;

  const Entity donor = world.createSdfGrid(Pose{}, blank());
  const Entity taker = world.createSdfGrid(Pose{}, blank());

  dunya::field::SampledSdf field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledSdf(donor, std::move(field));

  REQUIRE(world.sampledSdfUsers(donor) == 1);

  world.shareSampledSdf(donor, taker);

  REQUIRE(world.sampledSdf(taker) == world.sampledSdf(donor));
  REQUIRE(world.sampledSdfUsers(donor) == 2);
  REQUIRE(world.sampledSdfUsers(taker) == 2);

  REQUIRE_FALSE(world.needsResample(taker));
}

TEST_CASE("a dent on a shared lattice takes a private copy", "[world]") {
  World world;

  const Entity donor = world.createSdfGrid(Pose{}, blank());
  const Entity taker = world.createSdfGrid(Pose{}, blank());

  for (const Entity entity : {donor, taker}) {
    world.emplaceOrReplace<dunya::objectmodel::Deformable>(
      entity,
      dunya::objectmodel::Deformable{}
    );
  }

  dunya::field::SampledSdf field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledSdf(donor, std::move(field));
  world.shareSampledSdf(donor, taker);

  const dunya::field::SampledSdf* before = world.sampledSdf(donor);

  world.patchSampledSdf(taker, [](dunya::field::SampledSdf& lattice) {
    lattice.distances[0] = -5.0f;
  });

  REQUIRE(world.sampledSdf(taker) != before);
  REQUIRE(world.sampledSdf(donor) == before);

  REQUIRE_THAT(
    world.sampledSdf(taker)->distances[0],
    WithinAbs(-5.0f, ANALYTIC_TOLERANCE)
  );

  REQUIRE_THAT(
    world.sampledSdf(donor)->distances[0],
    WithinAbs(1.0f, ANALYTIC_TOLERANCE)
  );

  REQUIRE(world.sampledSdfUsers(donor) == 1);
  REQUIRE(world.sampledSdfUsers(taker) == 1);
}

TEST_CASE("a dent on an unshared lattice copies nothing", "[world]") {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );

  dunya::field::SampledSdf field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledSdf(entity, std::move(field));

  const dunya::field::SampledSdf* before = world.sampledSdf(entity);

  world.patchSampledSdf(entity, [](dunya::field::SampledSdf& lattice) {
    lattice.distances[0] = -5.0f;
  });

  REQUIRE(world.sampledSdf(entity) == before);
}

TEST_CASE("a dent records that the lattice left its primitives", "[world]") {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );

  dunya::field::SampledSdf field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledSdf(entity, std::move(field));

  REQUIRE_FALSE(world.registry().all_of<dunya::objectmodel::Deformed>(entity));

  world.patchSampledSdf(entity, [](dunya::field::SampledSdf& lattice) {
    lattice.distances[0] = -1.0f;
  });

  REQUIRE(world.registry().all_of<dunya::objectmodel::Deformed>(entity));
}

TEST_CASE("a fresh bake puts the lattice back on its primitives", "[world]") {
  World world;

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  world.emplaceOrReplace<dunya::objectmodel::Deformable>(
    entity,
    dunya::objectmodel::Deformable{}
  );

  dunya::field::SampledSdf field;
  field.resolution = glm::uvec3(2u);
  field.distances.assign(8u, 1.0f);
  field.materials.assign(8u, 0u);

  world.setSampledSdf(entity, field);

  world.patchSampledSdf(entity, [](dunya::field::SampledSdf& lattice) {
    lattice.distances[0] = -1.0f;
  });

  REQUIRE(world.registry().all_of<dunya::objectmodel::Deformed>(entity));

  world.setSampledSdf(entity, field);

  REQUIRE_FALSE(world.registry().all_of<dunya::objectmodel::Deformed>(entity));
}

TEST_CASE("clearing a world releases its entities and arena", "[world]") {
  World world;

  dunya::objectmodel::SdfGrid grid{};
  grid.resolution = glm::uvec3(65u);

  for (int i = 0; i != 3; ++i) {
    const dunya::objectmodel::Entity entity =
      world.createSdfGrid(dunya::objectmodel::Pose{}, grid);

    REQUIRE(world.addPrimitive(
      entity,
      dunya::field::makeBox(glm::vec3(0.0f), glm::vec3(0.5f))
    ));
  }

  REQUIRE(world.sdfGrids().size() == 3);

  const size_t held = world.pool().size();

  world.clear();

  REQUIRE(world.sdfGrids().empty());

  const auto* poses = world.registry().storage<dunya::objectmodel::Pose>();

  REQUIRE((poses == nullptr || poses->empty()));

  const dunya::objectmodel::Entity fresh =
    world.createSdfGrid(dunya::objectmodel::Pose{}, grid);

  REQUIRE(world.addPrimitive(
    fresh,
    dunya::field::makeBox(glm::vec3(0.0f), glm::vec3(0.5f))
  ));

  REQUIRE(world.sdfGrids().size() == 1);
  REQUIRE(world.pool().size() <= held);
}

TEST_CASE("the world's environment is found by lowest entity", "[worldquery]") {
  using dunya::objectmodel::DirectionalLight;
  using dunya::objectmodel::Environment;

  World world;

  REQUIRE(
    dunya::objectmodel::firstWith<DirectionalLight>(world)
    == dunya::objectmodel::INVALID_ENTITY
  );
  REQUIRE(
    dunya::objectmodel::firstWith<Environment>(world)
    == dunya::objectmodel::INVALID_ENTITY
  );

  const Entity first = world.createAuthored();
  const Entity second = world.createAuthored();

  world.emplaceAuthored(
    second,
    DirectionalLight{glm::vec3(0.0f, 1.0f, 0.0f), 0.5f}
  );
  world.emplaceAuthored(
    first,
    DirectionalLight{glm::vec3(0.0f, 1.0f, 0.0f), 0.25f}
  );

  const Entity found = dunya::objectmodel::firstWith<DirectionalLight>(world);

  REQUIRE(found == first);
  REQUIRE(world.registry().get<DirectionalLight>(found).ambient == 0.25f);

  Environment lit{};
  lit.exposure = 2.0f;

  world.emplaceAuthored(second, lit);

  REQUIRE(dunya::objectmodel::firstWith<Environment>(world) == second);
}

TEST_CASE("an authored camera is the world's active camera", "[worldquery]") {
  using dunya::objectmodel::activeCamera;
  using dunya::objectmodel::Lens;

  World world;

  REQUIRE_FALSE(activeCamera(world, 1.5f).has_value());

  const Entity eye = world.createAuthored();

  Pose seat{};
  seat.position = glm::vec3(3.0f, 4.0f, 5.0f);

  Lens lens{};
  lens.nearPlane = 0.25f;

  world.emplaceAuthored(eye, seat);
  world.emplaceAuthored(eye, lens);

  const auto resolved = activeCamera(world, 1.5f);

  REQUIRE(resolved.has_value());
  REQUIRE(resolved->position == seat.position);
  REQUIRE(resolved->nearPlane == 0.25f);
  REQUIRE(resolved->view == dunya::objectmodel::view(seat));
  REQUIRE(resolved->projection == dunya::objectmodel::projection(lens, 1.5f));

  const glm::vec3 eyeInView =
    glm::vec3(resolved->view * glm::vec4(seat.position, 1.0f));

  REQUIRE_THAT(glm::length(eyeInView), Catch::Matchers::WithinAbs(0.0, 1e-5));
}

TEST_CASE("dirty sdf regions merge per entity", "[world]") {
  World world;

  REQUIRE(world.sdfDirty().empty());

  const Entity first = world.createAuthored();
  const Entity second = world.createAuthored();

  dunya::field::SampleBox low{};
  low.minimum = glm::uvec3(2u, 2u, 2u);
  low.extent = glm::uvec3(3u, 3u, 3u);

  dunya::field::SampleBox high{};
  high.minimum = glm::uvec3(10u, 1u, 4u);
  high.extent = glm::uvec3(2u, 2u, 2u);

  world.markSdfDirty(first, low);
  world.markSdfDirty(first, high);
  world.markSdfDirty(second, low);

  REQUIRE(world.sdfDirty().size() == 2);

  const dunya::field::SampleBox& merged = world.sdfDirty()[0].second;

  REQUIRE(world.sdfDirty()[0].first == first);
  REQUIRE(merged.minimum == glm::uvec3(2u, 1u, 2u));
  REQUIRE(merged.minimum.x + merged.extent.x >= 12u);

  world.clearSdfDirty();

  REQUIRE(world.sdfDirty().empty());
}

TEST_CASE("a released baked volume reports its slot once", "[world]") {
  World world;

  std::vector<uint32_t> released;

  world.onBakedVolumeReleased([&released](uint32_t slot) {
    released.push_back(slot);
  });

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  world.setBakedVolume(entity, 7);

  REQUIRE(released.empty());

  REQUIRE(world.destroy(entity));

  REQUIRE(released == std::vector<uint32_t>{7});
}

TEST_CASE("replacing a baked volume releases the slot it held", "[world]") {
  World world;

  std::vector<uint32_t> released;

  world.onBakedVolumeReleased([&released](uint32_t slot) {
    released.push_back(slot);
  });

  const Entity entity = world.createSdfGrid(Pose{}, blank());

  world.setBakedVolume(entity, 3);
  world.setBakedVolume(entity, 9);

  REQUIRE(released == std::vector<uint32_t>{3});

  REQUIRE(world.registry().get<BakedVolume>(entity).index == 9);
}

TEST_CASE("clearing the baked volumes releases every slot", "[world]") {
  World world;

  std::vector<uint32_t> released;

  world.onBakedVolumeReleased([&released](uint32_t slot) {
    released.push_back(slot);
  });

  const Entity first = world.createSdfGrid(Pose{}, blank());
  const Entity second = world.createSdfGrid(Pose{}, blank());

  world.setBakedVolume(first, 1);
  world.setBakedVolume(second, 2);

  world.clearBakedVolumes();

  std::sort(released.begin(), released.end());

  REQUIRE(released == std::vector<uint32_t>{1, 2});
  REQUIRE_FALSE(world.registry().all_of<BakedVolume>(first));
  REQUIRE_FALSE(world.registry().all_of<BakedVolume>(second));
}
