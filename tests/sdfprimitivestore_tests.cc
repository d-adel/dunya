#include <catch2/catch_test_macros.hpp>

#include <dunya/core/config/config.h>
#include <dunya/objectmodel/fieldobject/fieldobject.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>

#include <entt/entity/registry.hpp>

#include <cstdint>

namespace {

using dunya::objectmodel::FieldObject;
using dunya::objectmodel::Entity;
using dunya::objectmodel::SdfPrimitiveRange;
using dunya::objectmodel::SdfPrimitiveStore;

// Materials number the primitives 1, 2, 3..., which is how a test tells one
// from another after an edit has shifted them.
dunya::field::Primitive marker(uint32_t material) {
  return dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, material);
}

Entity makeObject(entt::registry& registry) {
  const Entity entity = registry.create();

  FieldObject object{};
  object.resolution = glm::uvec3(dunya::core::FIELD_GRID_RESOLUTION);

  registry.emplace<FieldObject>(entity, object);

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

TEST_CASE("primitives need a field object to describe", "[sdfstore]") {
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

  store.append(registry, entity, marker(1));
  store.append(registry, entity, marker(3));

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

  store.append(registry, entity, marker(1));
  store.append(registry, entity, marker(2));
  store.append(registry, entity, marker(3));

  REQUIRE(store.remove(registry, entity, 1));

  REQUIRE(store.count(registry, entity) == 2);
  REQUIRE(materialAt(store, registry, entity, 0) == 1);
  REQUIRE(materialAt(store, registry, entity, 1) == 3);
}

TEST_CASE("setting replaces in place", "[sdfstore]") {
  entt::registry registry;
  SdfPrimitiveStore store;

  const Entity entity = makeObject(registry);

  store.append(registry, entity, marker(1));
  store.append(registry, entity, marker(2));

  REQUIRE(store.set(registry, entity, 0, marker(9)));

  REQUIRE(store.count(registry, entity) == 2);
  REQUIRE(materialAt(store, registry, entity, 0) == 9);
  REQUIRE(materialAt(store, registry, entity, 1) == 2);
}

TEST_CASE("crossing the capacity boundary preserves the primitives",
          "[sdfstore]") {
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

TEST_CASE("an edit marks the object dirty and refreshes its box",
          "[sdfstore]") {
  entt::registry registry;
  SdfPrimitiveStore store;

  const Entity entity = makeObject(registry);

  registry.get<FieldObject>(entity).dirty = false;

  REQUIRE(store.append(registry, entity, marker(1)));

  REQUIRE(registry.get<FieldObject>(entity).dirty);
  REQUIRE(registry.get<FieldObject>(entity).voxelSize.x > 0.0f);
}

TEST_CASE("destroying an entity returns its range to the arena",
          "[sdfstore]") {
  // The invariant the destruction signal exists for. A leak here is silent:
  // the arena simply never hears that the range is free.
  entt::registry registry;
  SdfPrimitiveStore store;

  store.connect(registry);

  const Entity entity = makeObject(registry);
  store.append(registry, entity, marker(1));

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
  store.append(registry, entity, marker(1));

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

  store.append(registry, first, marker(1));
  store.append(registry, second, marker(2));

  REQUIRE(store.pool().size() == 8);

  registry.clear();

  REQUIRE(store.pool().size() == 0);
}
