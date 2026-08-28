#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/core/config/config.h>
#include <dunya/objectmodel/fieldgrid/fieldgrid.h>
#include <dunya/objectmodel/pose/pose.h>
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
using dunya::objectmodel::FieldGrid;
using dunya::objectmodel::Pose;
using dunya::objectmodel::World;

// Materials number the primitives 1, 2, 3..., which is how a test tells one
// from another after an edit has shifted them.
dunya::field::Primitive marker(uint32_t material) {
  return dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, material);
}

// fitToPrimitives divides by the resolution, so a grid is only usable once it
// has one.
FieldGrid blank() {
  FieldGrid object{};

  object.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  return object;
}

const FieldGrid& gridOf(const World& world, Entity entity) {
  return world.registry().get<FieldGrid>(entity);
}

const Pose& poseOf(const World& world, Entity entity) {
  return world.registry().get<Pose>(entity);
}

uint32_t materialAt(const World& world, Entity entity, uint32_t index) {
  return world.primitives(entity)[index].shapeConfig.y;
}

// Entities in use, not components. An orphan entity carries nothing, so it is
// invisible to every other read in this file.
//
// A pointer because that is what the const overload returns, and never null
// for the entity type: assure() hands back the registry's own member rather
// than looking in the pools.
uint32_t liveEntityCount(const World& world) {
  return static_cast<uint32_t>(world.registry().storage<Entity>()->free_list());
}

}  // namespace

TEST_CASE("a created field is live and listed", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  // The dirty flag this replaced defaulted to true, so creation has always
  // implied a first bake.
  REQUIRE(world.needsBake(entity));

  REQUIRE(world.registry().valid(entity));
  REQUIRE(world.registry().all_of<FieldGrid>(entity));

  REQUIRE(world.fields().size() == 1);
  REQUIRE(world.fields()[0] == entity);
}

// The const-only accessor is the transaction boundary. A non-const overload
// would put every field back within reach of every caller, so it is pinned
// here rather than left to review.
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

  // The same value, version included, which is what undo needs: a command
  // holding this entity must still address the object it restored.
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

  // EnTT recycles the freed slot, so this add takes the identity a redo would
  // have asked for.
  const Entity recycled = world.createField(Pose{}, blank());

  const uint32_t before = liveEntityCount(world);

  REQUIRE_FALSE(world.createFieldAt(first, Pose{}, blank()));

  // create(hint) does not fail, it substitutes. The substitute must not
  // survive the refusal, as an object or as a bare entity.
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

  // The range sat at the end of the arena, so releasing it shrinks the pool
  // rather than leaving a hole. A pool that stays put means the destroy signal
  // never reached the store and the allocation leaked.
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

  // registry.destroy on a dead entity is a precondition violation, so the
  // refusal has to happen before it.
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

  const FieldGrid& object = gridOf(world, entity);

  REQUIRE(world.needsBake(entity));

  // A unit sphere at the origin, plus the grid margin on every side.
  const float expected = -(1.0f + dunya::core::FIELD_GRID_MARGIN);

  REQUIRE_THAT(object.origin.x, WithinAbs(expected, ANALYTIC_TOLERANCE));
  REQUIRE(object.voxelSize.x > 0.0f);
}

TEST_CASE("setPose writes position and rotation together", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  const glm::vec3 position(1.0f, 2.0f, 3.0f);

  const glm::quat rotation =
    glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  world.setPose(entity, position, rotation);

  const Pose& pose = poseOf(world, entity);

  REQUIRE_THAT(pose.position.x, WithinAbs(1.0f, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(pose.position.z, WithinAbs(3.0f, ANALYTIC_TOLERANCE));

  REQUIRE_THAT(pose.rotation.w, WithinAbs(rotation.w, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(pose.rotation.y, WithinAbs(rotation.y, ANALYTIC_TOLERANCE));
}

TEST_CASE("the component setters reach the object", "[world]") {
  World world;

  const Entity entity = world.createField(Pose{}, blank());

  // A fresh field entity owns no pool slot, and that is said by the component
  // not being there at all rather than by a sentinel value inside one. The
  // frame loop reads exactly this to decide whether to allocate.
  REQUIRE_FALSE(world.registry().all_of<BakedVolume>(entity));

  world.setBakedVolume(entity, 3);

  REQUIRE(world.registry().all_of<BakedVolume>(entity));
  REQUIRE(world.registry().get<BakedVolume>(entity).index == 3);
}

TEST_CASE(
  "setting a baked volume twice replaces rather than throws",
  "[world]"
) {
  // emplace_or_replace, not emplace: a re-bake into a different pool slot must
  // overwrite the old number. emplace would abort on the second call.
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

  // Swap-and-pop storage, so the span holds live entities only. If FieldGrid
  // ever needs stable storage the dense array gains tombstones and this fails.
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
