#include <dunya/view/viewportstore/viewportstore.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using dunya::view::Viewport;
using dunya::view::ViewportStore;

namespace {

constexpr float TOLERANCE = 1e-6f;

Viewport configured(float supersample) {
  Viewport port{};
  port.supersample = supersample;
  port.gridVisible = true;

  return port;
}

}

TEST_CASE(
  "a created viewport is found and starts at its defaults",
  "[viewportstore]"
) {
  ViewportStore store;

  const auto id = store.create();
  const Viewport* port = store.find(id);

  REQUIRE(port != nullptr);
  REQUIRE(port->target == dunya::view::INVALID_TARGET);
  REQUIRE(port->camera == dunya::objectmodel::INVALID_ENTITY);
  REQUIRE_THAT(port->supersample, WithinAbs(1.0f, TOLERANCE));
  REQUIRE(store.count() == 1);
}

TEST_CASE("configuring replaces the whole viewport", "[viewportstore]") {
  ViewportStore store;

  const auto id = store.create();

  REQUIRE(store.configure(id, configured(2.0f)));

  const Viewport* port = store.find(id);

  REQUIRE(port != nullptr);
  REQUIRE_THAT(port->supersample, WithinAbs(2.0f, TOLERANCE));
  REQUIRE(port->gridVisible);
}

TEST_CASE("viewports do not share identity", "[viewportstore]") {
  ViewportStore store;

  const auto first = store.create();
  const auto second = store.create();

  REQUIRE(first != second);
  REQUIRE(store.configure(first, configured(3.0f)));

  REQUIRE_THAT(store.find(first)->supersample, WithinAbs(3.0f, TOLERANCE));
  REQUIRE_THAT(store.find(second)->supersample, WithinAbs(1.0f, TOLERANCE));
  REQUIRE(store.count() == 2);
}

TEST_CASE(
  "a destroyed viewport is gone and cannot be reached",
  "[viewportstore]"
) {
  ViewportStore store;

  const auto id = store.create();

  REQUIRE(store.destroy(id));
  REQUIRE(store.find(id) == nullptr);
  REQUIRE(store.count() == 0);

  REQUIRE_FALSE(store.destroy(id));
  REQUIRE_FALSE(store.configure(id, configured(2.0f)));
}

TEST_CASE(
  "a destroyed slot is reused and comes back blank",
  "[viewportstore]"
) {
  ViewportStore store;

  const auto first = store.create();

  REQUIRE(store.configure(first, configured(4.0f)));
  REQUIRE(store.destroy(first));

  const auto reused = store.create();

  REQUIRE(reused == first);
  REQUIRE_THAT(store.find(reused)->supersample, WithinAbs(1.0f, TOLERANCE));
  REQUIRE_FALSE(store.find(reused)->gridVisible);
}

TEST_CASE(
  "destroying one viewport leaves the others alone",
  "[viewportstore]"
) {
  ViewportStore store;

  const auto first = store.create();
  const auto second = store.create();
  const auto third = store.create();

  REQUIRE(store.configure(third, configured(5.0f)));
  REQUIRE(store.destroy(second));

  REQUIRE(store.find(first) != nullptr);
  REQUIRE(store.find(second) == nullptr);
  REQUIRE(store.find(third) != nullptr);
  REQUIRE_THAT(store.find(third)->supersample, WithinAbs(5.0f, TOLERANCE));
  REQUIRE(store.count() == 2);
}

TEST_CASE("an id nobody handed out reaches nothing", "[viewportstore]") {
  ViewportStore store;

  const auto id = store.create();

  REQUIRE(store.find(id + 1u) == nullptr);
  REQUIRE(store.find(dunya::view::INVALID_VIEWPORT) == nullptr);
  REQUIRE_FALSE(store.destroy(dunya::view::INVALID_VIEWPORT));
}
