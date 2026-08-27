#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>

#include "tolerances.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;
using dunya::field::AnalyticSample;
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

        const AnalyticSample exact = dunya::field::sample(primitives, point);

        REQUIRE_THAT(
          dunya::field::distance(field, point),
          WithinAbs(exact.distance, ANALYTIC_TOLERANCE)
        );
        REQUIRE(dunya::field::material(field, point) == exact.material);
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
        const float baked = dunya::field::distance(field, point);

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
        const float baked = dunya::field::distance(field, point);

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
    const float outside = dunya::field::distance(field, point);

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
    const uint32_t baked =
      dunya::field::material(field, glm::vec3(x, 0.05f, -0.05f));

    REQUIRE((baked == 2u || baked == 9u));
  }
}

TEST_CASE("the global bound is the largest brick bound", "[sampled][bound]") {
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  const SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  // 32 cells on each axis at eight cells to a brick.
  REQUIRE(field.brickLipschitz.size() == 64u);

  float worst = 0.0f;
  for (float bound : field.brickLipschitz) {
    worst = std::max(worst, bound);
  }

  REQUIRE_THAT(field.globalLipschitz, WithinAbs(worst, ANALYTIC_TOLERANCE));

  // The corner brick sits far outside the sphere where the field is smooth, and
  // its bound is the 1 a distance field is meant to have. Measured at 1.0400.
  REQUIRE(field.brickLipschitz[0] > 0.95f);
  REQUIRE(field.brickLipschitz[0] < 1.10f);

  // The global one is root three, and not by accident: a distance field has a
  // cone tip at the sphere's centre, where all three axis slopes reach one at
  // once. It is the crease case, sitting deep inside the solid where no march
  // ever goes - which is the argument for per-brick bounds over one global one.
  REQUIRE_THAT(field.globalLipschitz, WithinAbs(1.7320508f, 1e-4f));
}

TEST_CASE("the sampled gradient is the interpolant's own", "[sampled][gradient]") {
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  const SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  const glm::vec3 probes[] = {
    glm::vec3(1.5f, 0.13f, -0.07f),
    glm::vec3(-0.11f, 1.42f, 0.23f),
    glm::vec3(0.31f, -0.19f, -1.61f),
    glm::vec3(0.83f, 0.79f, 0.71f)
  };

  for (const glm::vec3& probe : probes) {
    const glm::vec3 g = dunya::field::gradient(field, probe);

    // A distance field's gradient has magnitude one, and points away from the
    // sphere's centre. The interpolant keeps both to within a voxel's worth.
    REQUIRE_THAT(glm::length(g), WithinAbs(1.0f, 0.05f));
    REQUIRE(glm::dot(glm::normalize(g), glm::normalize(probe)) > 0.99f);
  }
}

TEST_CASE("a stepBound step never crosses the surface", "[sampled][bound]") {
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  const SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  const glm::vec3 targets[] = {
    glm::vec3(0.0f),
    glm::vec3(0.94f, 0.0f, 0.0f),
    glm::vec3(0.0f, -0.97f, 0.12f),
    glm::vec3(0.6f, 0.6f, 0.6f)
  };

  uint32_t stepsTaken = 0;

  for (float a = 0.0f; a < 6.2f; a += 0.4f) {
    for (float b = -1.2f; b < 1.3f; b += 0.6f) {
      const glm::vec3 origin(
        1.9f * std::cos(a) * std::cos(b),
        1.9f * std::sin(b),
        1.9f * std::sin(a) * std::cos(b)
      );

      for (const glm::vec3& target : targets) {
        const glm::vec3 direction = glm::normalize(target - origin);

        glm::vec3 point = origin;

        for (int i = 0; i < 64; ++i) {
          const float before = dunya::field::distance(field, point);

          if (before <= 0.0f) {
            break;
          }

          const float step =
            dunya::field::stepBound(field, point, direction);

          // The whole guarantee, stated directly: no zero of the field lies in
          // the step. Checking only where it lands would miss a thin negative
          // region the ray passed straight through.
          for (int s = 1; s <= 16; ++s) {
            const glm::vec3 along =
              point + direction * (step * static_cast<float>(s) / 16.0f);

            REQUIRE(dunya::field::distance(field, along) > -ANALYTIC_TOLERANCE);
          }

          point += direction * step;
          ++stepsTaken;
        }
      }
    }
  }

  REQUIRE(stepsTaken > 1000u);
}

TEST_CASE("a step stops at the wall of the brick it was measured in",
          "[sampled][bound]") {
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  // A carve written into one brick puts a surface inside it that the
  // neighbouring brick's samples know nothing about: its own values still
  // describe an empty stretch, so only the wall can stop the step.
  const dunya::field::SampleBox carve{glm::uvec3(12u, 30u, 16u), glm::uvec3(1u)};
  const std::vector<float> inside{-5.0f};
  const std::vector<uint32_t> material{3u};

  dunya::field::write(field, carve, inside, material);

  // Just inside the first brick, a twentieth of a unit from its wall, while the
  // sphere is a full unit away.
  const glm::vec3 point(-1.05f, 1.75f, 0.0f);
  const glm::vec3 direction(1.0f, 0.0f, 0.0f);

  REQUIRE(dunya::field::distance(field, point) > 1.0f);

  const float step = dunya::field::stepBound(field, point, direction);

  for (int s = 1; s <= 64; ++s) {
    const glm::vec3 along =
      point + direction * (step * static_cast<float>(s) / 64.0f);

    REQUIRE(dunya::field::distance(field, along) > -ANALYTIC_TOLERANCE);
  }
}

TEST_CASE("a step never shrinks to nothing at a brick wall", "[sampled][bound]") {
  // A ray that starts beside a wall has almost no room before it, and a step
  // that small can land short of the wall again, in the brick it never left.
  // The march then stops advancing - which no assertion about crossing the
  // surface can see, because a ray that does not move never crosses anything.
  // Whether the rounding falls short is not reproducible, so what is pinned
  // here is the guard rather than the symptom it prevents.
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  const SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  // Bricks are eight cells, so on this grid their walls fall on whole units.
  // The ray sits a ten-thousandth inside one, well clear of the sphere, so the
  // brick's exit is what limits the step and it is nearly nothing.
  const glm::vec3 direction(1.0f, 0.0f, 0.0f);
  const glm::vec3 point(-1.0001f, 1.8f, 0.0f);

  const float step = dunya::field::stepBound(field, point, direction);

  const glm::vec3 voxel = field.voxelSize;
  const float smallest = std::min(voxel.x, std::min(voxel.y, voxel.z));

  REQUIRE(step >= 0.5f * smallest);

  // And the floor is a floor, not a licence: it never exceeds the distance the
  // field itself allows.
  REQUIRE(step <= dunya::field::distance(field, point));
}

TEST_CASE("a flat brick does not license a step into a carved neighbour",
          "[sampled][bound]") {
  // The adversarial case for the progress floor. A ray sits a whisker inside
  // one brick with the next one carved open a single cell past the wall. The
  // floor lets it cross, so the crossing has to be inside what this brick's
  // bound describes - which is why a brick reduces over a one-cell halo rather
  // than its own cells alone.
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  // Lattice 16 is the wall between the second and third brick; sample 17 is one
  // cell past it. Well above the sphere, so nothing else is near.
  const dunya::field::SampleBox carve{glm::uvec3(17u, 30u, 16u), glm::uvec3(1u)};
  const std::vector<float> inside{-5.0f};
  const std::vector<uint32_t> material{3u};

  dunya::field::write(field, carve, inside, material);

  const float wall = -2.0f + 16.0f * field.voxelSize.x;
  const glm::vec3 point(wall - 0.0001f, 1.75f, 0.0f);
  const glm::vec3 direction(1.0f, 0.0f, 0.0f);

  REQUIRE(dunya::field::distance(field, point) > 0.0f);

  const float step = dunya::field::stepBound(field, point, direction);

  for (int s = 1; s <= 64; ++s) {
    const glm::vec3 along =
      point + direction * (step * static_cast<float>(s) / 64.0f);

    REQUIRE(dunya::field::distance(field, along) > -ANALYTIC_TOLERANCE);
  }
}

TEST_CASE("a write rebuilds the bricks on both sides of a boundary",
          "[sampled][write]") {
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 3)};

  SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(17u)
  );

  const std::vector<float> before = field.brickLipschitz;

  // Sample eight sits on the boundary: it belongs to cell seven, in the first
  // brick, and to cell eight, in the second.
  const dunya::field::SampleBox box{glm::uvec3(8u, 4u, 4u), glm::uvec3(1u)};
  const std::vector<float> spike{5.0f};
  const std::vector<uint32_t> material{3u};

  dunya::field::write(field, box, spike, material);

  const uint32_t touched = 0u;      // brick (0, 0, 0)
  const uint32_t neighbour = 1u;    // brick (1, 0, 0)
  const uint32_t untouched = 6u;    // brick (0, 1, 1)

  REQUIRE(field.brickLipschitz[touched] > before[touched]);
  REQUIRE(field.brickLipschitz[neighbour] > before[neighbour]);
  REQUIRE_THAT(
    field.brickLipschitz[untouched],
    WithinAbs(before[untouched], ANALYTIC_TOLERANCE)
  );

  REQUIRE_THAT(
    field.globalLipschitz,
    WithinAbs(
      std::max(field.brickLipschitz[touched], field.brickLipschitz[neighbour]),
      ANALYTIC_TOLERANCE
    )
  );
}
