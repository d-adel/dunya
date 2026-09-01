#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/core/config/config.h>
#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>

#include "tolerances.h"

#include <span>
#include <vector>

using Catch::Matchers::WithinAbs;
using dunya::field::Aabb;
using dunya::field::Primitive;
using dunya::objectmodel::SdfGrid;

namespace {

std::vector<Primitive> sphere(float radius) {
  return {dunya::field::makeSphere(glm::vec3(0.0f), radius)};
}

SdfGrid fitted(std::span<const Primitive> primitives, uint32_t resolution) {
  SdfGrid grid{};
  grid.resolution = glm::uvec3(resolution);

  dunya::objectmodel::fitToPrimitives(grid, primitives);

  return grid;
}

float marginInCells(float radius, uint32_t resolution) {
  const std::vector<Primitive> solid = sphere(radius);

  const SdfGrid grid = fitted(solid, resolution);

  const float margin = dunya::objectmodel::gridMargin(grid, solid);

  const float coarsest =
    std::max({grid.voxelSize.x, grid.voxelSize.y, grid.voxelSize.z});

  return margin / coarsest;
}

}

TEST_CASE("an unauthored margin is the same cells at any size", "[bounds]") {
  const float cells = float(dunya::core::FIELD_GRID_MARGIN_CELLS);

  REQUIRE_THAT(marginInCells(0.15f, 33u), WithinAbs(cells, MARCH_TOLERANCE));
  REQUIRE_THAT(marginInCells(1.5f, 33u), WithinAbs(cells, MARCH_TOLERANCE));
  REQUIRE_THAT(marginInCells(12.0f, 33u), WithinAbs(cells, MARCH_TOLERANCE));
}

TEST_CASE(
  "an unauthored margin is the same cells at any resolution",
  "[bounds]"
) {
  const float cells = float(dunya::core::FIELD_GRID_MARGIN_CELLS);

  REQUIRE_THAT(marginInCells(0.15f, 17u), WithinAbs(cells, MARCH_TOLERANCE));
  REQUIRE_THAT(marginInCells(0.15f, 65u), WithinAbs(cells, MARCH_TOLERANCE));
  REQUIRE_THAT(marginInCells(0.15f, 128u), WithinAbs(cells, MARCH_TOLERANCE));
}

TEST_CASE("a finer grid pads a smaller distance", "[bounds]") {
  const std::vector<Primitive> solid = sphere(0.15f);

  const float coarse =
    dunya::objectmodel::gridMargin(fitted(solid, 17u), solid);

  const float fine = dunya::objectmodel::gridMargin(fitted(solid, 128u), solid);

  REQUIRE(fine < coarse);
  REQUIRE(fine > 0.0f);
}

TEST_CASE("an authored margin wins over the fitted one", "[bounds]") {
  const std::vector<Primitive> solid = sphere(0.15f);

  SdfGrid grid = fitted(solid, 33u);
  grid.margin = 2.0f;

  REQUIRE_THAT(
    dunya::objectmodel::gridMargin(grid, solid),
    WithinAbs(2.0f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("the caster box never sits inside the grid box", "[bounds]") {
  for (const float radius : {0.15f, 1.5f, 12.0f}) {
    const std::vector<Primitive> solid = sphere(radius);

    const SdfGrid grid = fitted(solid, 33u);

    const Aabb caster = dunya::objectmodel::casterBox(solid, grid);

    const Aabb box = dunya::objectmodel::gridBox(
      solid,
      dunya::objectmodel::gridMargin(grid, solid)
    );

    REQUIRE(caster.maximum.x >= box.maximum.x);
    REQUIRE(caster.minimum.y <= box.minimum.y);
  }
}

TEST_CASE("an unauthored grid stores no margin of its own", "[bounds]") {
  const SdfGrid grid{};

  REQUIRE(!grid.margin.has_value());
  REQUIRE(!grid.shadowCullMargin.has_value());
}
