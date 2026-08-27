/* M31 step 0. Proves the dependency compiles, links and behaves, and pins the
 * one property that decided the design: an entity handle is not a dense index.
 */

#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include <cstdint>

namespace {

struct Marker {
  float value;
};

}  // namespace

TEST_CASE("a component round trips through a registry", "[entt]") {
  entt::registry registry;

  const entt::entity entity = registry.create();
  registry.emplace<Marker>(entity, 42.0f);

  REQUIRE(registry.valid(entity));
  REQUIRE(registry.get<Marker>(entity).value == 42.0f);
}

TEST_CASE("a handle to a destroyed entity is detectable", "[entt]") {
  // This is what the old ObjectId could not do: a recycled id was
  // indistinguishable from the one it replaced, so a stale handle read whatever
  // took its place.
  entt::registry registry;

  const entt::entity entity = registry.create();
  registry.destroy(entity);

  REQUIRE_FALSE(registry.valid(entity));
}

TEST_CASE("a recycled entity is not the same number", "[entt]") {
  // The property that broke `ObjectId == GPU slot`. EnTT reuses the index and
  // bumps a version packed into the same 32 bits, so the numeric value of a
  // handle is not small, not dense, and not a subscript.
  entt::registry registry;

  const entt::entity first = registry.create();
  registry.destroy(first);

  const entt::entity second = registry.create();

  REQUIRE(registry.valid(second));
  REQUIRE_FALSE(registry.valid(first));
  REQUIRE(
    static_cast<std::uint32_t>(second) != static_cast<std::uint32_t>(first)
  );
}
