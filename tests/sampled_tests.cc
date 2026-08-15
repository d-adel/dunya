#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "field/analytic.h"
#include "field/field.h"
#include "field/sampled.h"

#include "tolerances.h"

#include <glm/gtc/matrix_transform.hpp>

#include <vector>

using Catch::Matchers::WithinAbs;
using dunya::field::FieldSample;
using dunya::field::Primitive;
using dunya::field::SampledField;

namespace {

constexpr uint32_t SPHERE = 0;
constexpr uint32_t UNION = 0;

Primitive makeSphere(
  const glm::vec3& centre,
  float radius,
  uint32_t material,
  uint32_t operation = UNION
) {
  Primitive primitive{};
  primitive.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), centre));
  primitive.shape = glm::vec4(radius, 0.0f, 0.0f, 0.0f);
  primitive.shapeConfig = glm::uvec4(SPHERE, material, operation, 0);
  dunya::field::updateBounds(primitive);

  return primitive;
}

}  // namespace

TEST_CASE(
  "a bake reproduces the field exactly at lattice points",
  "[sampled]"
) {
  // This is the property that makes the whole comparison meaningful: the grid
  // is the analytic field, resampled. Any disagreement here is an indexing or
  // addressing bug, not an interpolation error.
  const std::vector<Primitive> primitives{
    makeSphere(glm::vec3(0.0f), 1.0f, 3),
    makeSphere(glm::vec3(1.2f, 0.3f, 0.0f), 0.6f, 5)
  };

  const glm::vec3 minimum(-2.0f);
  const glm::vec3 maximum(2.0f);
  const glm::uvec3 resolution(17u);

  const SampledField field =
    dunya::field::bake(primitives, minimum, maximum, resolution);

  for (uint32_t z = 0; z < resolution.z; ++z) {
    for (uint32_t y = 0; y < resolution.y; ++y) {
      for (uint32_t x = 0; x < resolution.x; ++x) {
        const glm::vec3 point =
          field.origin + field.voxelSize * glm::vec3(x, y, z);

        const FieldSample exact = dunya::field::sample(primitives, point);
        const FieldSample baked = dunya::field::sample(field, point);

        REQUIRE_THAT(
          baked.distance,
          WithinAbs(exact.distance, ANALYTIC_TOLERANCE)
        );
        REQUIRE(baked.material == exact.material);
      }
    }
  }
}

TEST_CASE(
  "interpolation error between lattice points is bounded",
  "[sampled]"
) {
  // Trilinear interpolation of a curved field undershoots the true distance.
  // The bound below is measured rather than assumed, and exists so a change to
  // the interpolation shows up as a number rather than as an impression.
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  const SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  float worst = 0.0f;

  for (float x = -1.9f; x <= 1.9f; x += 0.137f) {
    for (float y = -1.9f; y <= 1.9f; y += 0.137f) {
      for (float z = -1.9f; z <= 1.9f; z += 0.271f) {
        const glm::vec3 point(x, y, z);

        const float exact = dunya::field::sample(primitives, point).distance;
        const float baked = dunya::field::sample(field, point).distance;

        worst = std::max(worst, std::abs(baked - exact));
      }
    }
  }

  // Measured at 0.0114 for a unit sphere on a 33-point grid over a 4-unit box,
  // which is about three times what h^2/8 * f'' predicts. The bound is set
  // just above the measurement so a regression shows up as a number.
  REQUIRE(worst < 0.015f);
}

TEST_CASE("interpolation overestimates, so a march must not trust it") {
  // A distance field is convex away from its surface, and linear interpolation
  // of a convex function sits above it. The sampled distance can therefore
  // exceed the true one, which is exactly the non-Lipschitz behaviour that
  // makes plain sphere tracing step past a surface. This is the measurement
  // behind M17's D1 safety factor, not a defect to fix.
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  const SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  float worstOvershoot = 0.0f;

  for (float x = -1.9f; x <= 1.9f; x += 0.137f) {
    for (float y = -1.9f; y <= 1.9f; y += 0.137f) {
      for (float z = -1.9f; z <= 1.9f; z += 0.271f) {
        const glm::vec3 point(x, y, z);

        const float exact = dunya::field::sample(primitives, point).distance;
        const float baked = dunya::field::sample(field, point).distance;

        worstOvershoot = std::max(worstOvershoot, baked - exact);
      }
    }
  }

  // Overshoot is real, which is the point of the test.
  REQUIRE(worstOvershoot > 0.0f);
  REQUIRE(worstOvershoot < 0.015f);
}

TEST_CASE("outside the grid the distance is a lower bound", "[sampled]") {
  // A ray approaching from outside must never be told it can step further than
  // it really can, or it walks through the grid without ever sampling it.
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  const SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(17u)
  );

  // Every probe has to be genuinely beyond the box: a diagonal point can be
  // further from the origin than the box is wide and still be inside it, in
  // which case this would be testing interpolation rather than the bound.
  const std::vector<glm::vec3> outsidePoints{
    glm::vec3(3.0f, 0.0f, 0.0f),
    glm::vec3(-4.5f, 0.0f, 0.0f),
    glm::vec3(0.0f, 6.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, -2.5f),
    glm::vec3(3.0f, 3.0f, 3.0f),
    glm::vec3(-2.5f, 4.0f, -3.5f)
  };

  for (const glm::vec3& point : outsidePoints) {
    const float exact = dunya::field::sample(primitives, point).distance;
    const float outside = dunya::field::sample(field, point).distance;

    REQUIRE(outside > 0.0f);
    REQUIRE(outside <= exact + ANALYTIC_TOLERANCE);
  }
}

TEST_CASE("material ids are never interpolated", "[sampled]") {
  // Two touching spheres with distant ids. Every sample must return one of
  // them; a blend would invent an id that names nothing.
  const std::vector<Primitive> primitives{
    makeSphere(glm::vec3(-0.5f, 0.0f, 0.0f), 0.8f, 2),
    makeSphere(glm::vec3(0.5f, 0.0f, 0.0f), 0.8f, 9)
  };

  const SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(17u)
  );

  for (float x = -1.9f; x <= 1.9f; x += 0.083f) {
    const FieldSample baked =
      dunya::field::sample(field, glm::vec3(x, 0.05f, -0.05f));

    REQUIRE((baked.material == 2u || baked.material == 9u));
  }
}
