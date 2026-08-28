#include <catch2/catch_test_macros.hpp>

#include <dunya/core/config/config.h>
#include <dunya/objectmodel/fieldgrid/fieldgrid.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>

#include <cstdint>

namespace {

using dunya::objectmodel::Entity;
using dunya::objectmodel::FieldGrid;
using dunya::objectmodel::SdfPrimitiveRange;
using dunya::objectmodel::SdfPrimitiveStore;

// A reactive pool standing in for the one World opens. The store owes an
// on_update every time it refreshes derived state; this is what watches for it,
// and it is the whole of what the old dirty bool meant.
auto& bakeQueue(entt::registry& registry) {
  auto& queue = registry.storage<entt::reactive>(entt::hashed_string{"bake"});
  queue.on_update<FieldGrid>();
  return queue;
}

// Materials number the primitives 1, 2, 3..., which is how a test tells one
// from another after an edit has shifted them.
dunya::field::Primitive marker(uint32_t material) {
  return dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, material);
}

Entity makeObject(entt::registry& registry) {
  const Entity entity = registry.create();

  FieldGrid grid{};
  grid.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  registry.emplace<FieldGrid>(entity, grid);

  return entity;
}

uint32_t materialAt(
  const SdfPrimitiveStore& store,
  const entt::registry& registry,
  Entity entity,
  uint32_t index
) {
  return store.primitives(registry, entity)[index].shapeConfig.y;
}

}  // namespace

TEST_CASE("primitives need a grid to be sampled onto", "[sdfstore]") {
  entt::registry registry;
  SdfPrimitiveStore store;

  const Entity bare = registry.create();

  REQUIRE_FALSE(store.append(registry, bare, marker(1)));
  REQUIRE(store.count(registry, bare) == 0);
}

TEST_CASE("appending keeps order and grows the count", "[sdfstore]") {
  entt::registry registry;
  SdfPrimitiveStore store;

  const Entity entity = makeObject(registry);

  REQUIRE(store.append(registry, entity, marker(1)));
  REQUIRE(store.append(registry, entity, marker(2)));

  REQUIRE(store.count(registry, entity) == 2);
  REQUIRE(materialAt(store, registry, entity, 0) == 1);
  REQUIRE(materialAt(store, registry, entity, 1) == 2);
}

TEST_CASE("inserting shifts the primitives after it", "[sdfstore]") {
  // Order is what the CSG fold means, so an insert cannot swap-and-pop.
  entt::registry registry;
  SdfPrimitiveStore store;

  const Entity entity = makeObject(registry);

  REQUIRE(store.append(registry, entity, marker(1)));
  REQUIRE(store.append(registry, entity, marker(3)));

  REQUIRE(store.insert(registry, entity, 1, marker(2)));

  REQUIRE(store.count(registry, entity) == 3);
  REQUIRE(materialAt(store, registry, entity, 0) == 1);
  REQUIRE(materialAt(store, registry, entity, 1) == 2);
  REQUIRE(materialAt(store, registry, entity, 2) == 3);
}

TEST_CASE("removing shifts the primitives down", "[sdfstore]") {
  entt::registry registry;
  SdfPrimitiveStore store;

  const Entity entity = makeObject(registry);

  REQUIRE(store.append(registry, entity, marker(1)));
  REQUIRE(store.append(registry, entity, marker(2)));
  REQUIRE(store.append(registry, entity, marker(3)));

  REQUIRE(store.remove(registry, entity, 1));

  REQUIRE(store.count(registry, entity) == 2);
  REQUIRE(materialAt(store, registry, entity, 0) == 1);
  REQUIRE(materialAt(store, registry, entity, 1) == 3);
}

TEST_CASE("setting replaces in place", "[sdfstore]") {
  entt::registry registry;
  SdfPrimitiveStore store;

  const Entity entity = makeObject(registry);

  REQUIRE(store.append(registry, entity, marker(1)));
  REQUIRE(store.append(registry, entity, marker(2)));

  REQUIRE(store.set(registry, entity, 0, marker(9)));

  REQUIRE(store.count(registry, entity) == 2);
  REQUIRE(materialAt(store, registry, entity, 0) == 9);
  REQUIRE(materialAt(store, registry, entity, 1) == 2);
}

TEST_CASE(
  "crossing the capacity boundary preserves the primitives",
  "[sdfstore]"
) {
  // The first range holds four. The fifth append has to move the other four.
  entt::registry registry;
  SdfPrimitiveStore store;

  const Entity entity = makeObject(registry);

  for (uint32_t i = 0; i != 5; ++i) {
    REQUIRE(store.append(registry, entity, marker(i + 1)));
  }

  REQUIRE(store.count(registry, entity) == 5);
  REQUIRE(registry.get<SdfPrimitiveRange>(entity).capacity == 8);

  for (uint32_t i = 0; i != 5; ++i) {
    REQUIRE(materialAt(store, registry, entity, i) == i + 1);
  }
}

TEST_CASE(
  "an edit queues the grid for bake and re-fits its box",
  "[sdfstore]"
) {
  entt::registry registry;
  SdfPrimitiveStore store;

  auto& queue = bakeQueue(registry);

  const Entity entity = makeObject(registry);

  REQUIRE_FALSE(queue.contains(entity));

  REQUIRE(store.append(registry, entity, marker(1)));

  REQUIRE(queue.contains(entity));
  REQUIRE(registry.get<FieldGrid>(entity).voxelSize.x > 0.0f);
}

TEST_CASE("destroying an entity returns its range to the arena", "[sdfstore]") {
  // The invariant the destruction signal exists for. A leak here is silent:
  // the arena simply never hears that the range is free.
  entt::registry registry;
  SdfPrimitiveStore store;

  store.connect(registry);

  const Entity entity = makeObject(registry);
  REQUIRE(store.append(registry, entity, marker(1)));

  REQUIRE(store.pool().size() == 4);

  registry.destroy(entity);

  REQUIRE(store.pool().size() == 0);
}

TEST_CASE("removing the range component alone returns it too", "[sdfstore]") {
  // The path a World::destroy transaction would not have covered.
  entt::registry registry;
  SdfPrimitiveStore store;

  store.connect(registry);

  const Entity entity = makeObject(registry);
  REQUIRE(store.append(registry, entity, marker(1)));

  registry.remove<SdfPrimitiveRange>(entity);

  REQUIRE(store.pool().size() == 0);
  REQUIRE(registry.valid(entity));
}

TEST_CASE("clearing the registry returns every range", "[sdfstore]") {
  entt::registry registry;
  SdfPrimitiveStore store;

  store.connect(registry);

  const Entity first = makeObject(registry);
  const Entity second = makeObject(registry);

  REQUIRE(store.append(registry, first, marker(1)));
  REQUIRE(store.append(registry, second, marker(2)));

  REQUIRE(store.pool().size() == 8);

  registry.clear();

  REQUIRE(store.pool().size() == 0);
}

TEST_CASE(
  "clearing empties the primitives and keeps the allocation",
  "[sdfstore]"
) {
  // Distinct from removal: the range survives, so the arena does not shrink
  // and the next append needs no growth.
  entt::registry registry;
  SdfPrimitiveStore store;

  store.connect(registry);

  auto& queue = bakeQueue(registry);

  const Entity entity = makeObject(registry);

  REQUIRE(store.append(registry, entity, marker(1)));
  REQUIRE(store.append(registry, entity, marker(2)));
  REQUIRE(store.append(registry, entity, marker(3)));

  const size_t allocated = store.pool().size();

  queue.remove(entity);

  REQUIRE(store.clear(registry, entity));

  REQUIRE(store.count(registry, entity) == 0);
  REQUIRE(store.primitives(registry, entity).empty());
  REQUIRE(store.pool().size() == allocated);
  REQUIRE(queue.contains(entity));
}

TEST_CASE(
  "clearing an entity that holds no primitives is refused",
  "[sdfstore]"
) {
  entt::registry registry;
  SdfPrimitiveStore store;

  store.connect(registry);

  const Entity entity = makeObject(registry);

  REQUIRE_FALSE(store.clear(registry, entity));
}
