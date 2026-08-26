#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/field/field.h>
#include <dunya/field/raycast/raycast.h>

#include "tolerances.h"

#include <glm/gtc/matrix_transform.hpp>

#include <optional>
#include <utility>
#include <vector>

using Catch::Matchers::WithinAbs;
using dunya::field::Aabb;
using dunya::field::Primitive;
using dunya::field::Ray;
using dunya::field::RayHit;

namespace {

Primitive makeSphere(const glm::vec3& centre, float radius, uint32_t material) {
  Primitive primitive{};
  primitive.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), centre));
  primitive.shape = glm::vec4(radius, 0.0f, 0.0f, 0.0f);
  primitive.shapeConfig = glm::uvec4(0, material, 0, 0);

  return primitive;
}

}  // namespace

TEST_CASE("a ray through a projected point aims back at it", "[raycast]") {
  const glm::vec3 eye(3.0f, 2.0f, 5.0f);
  const glm::vec3 target(-1.0f, 0.5f, -2.0f);

  const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0, 1, 0));
  const glm::mat4 proj =
    glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

  const glm::mat4 viewProj = proj * view;

  // Project a known world point, then ask for the ray through where it landed.
  // Whatever this projection's handedness or y convention is, the ray has to
  // come back pointing at the point it came from — which is the property the
  // shader relies on and the one a wrong inverse would break.
  const glm::vec4 clip = viewProj * glm::vec4(target, 1.0f);
  const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);

  const Ray ray =
    dunya::field::screenPointToRay(glm::inverse(viewProj), eye, ndc);

  const glm::vec3 expected = glm::normalize(target - eye);

  REQUIRE_THAT(ray.origin.x, WithinAbs(eye.x, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(ray.origin.y, WithinAbs(eye.y, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(ray.origin.z, WithinAbs(eye.z, ANALYTIC_TOLERANCE));

  REQUIRE_THAT(ray.direction.x, WithinAbs(expected.x, GRADIENT_TOLERANCE));
  REQUIRE_THAT(ray.direction.y, WithinAbs(expected.y, GRADIENT_TOLERANCE));
  REQUIRE_THAT(ray.direction.z, WithinAbs(expected.z, GRADIENT_TOLERANCE));
}

TEST_CASE("a ray direction is always normalised", "[raycast]") {
  const glm::vec3 eye(0.0f, 0.0f, 4.0f);

  const glm::mat4 viewProj =
    glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f)
    * glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0, 1, 0));

  for (const glm::vec2 ndc :
       {glm::vec2(0.0f), glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, 1.0f)}) {
    const Ray ray =
      dunya::field::screenPointToRay(glm::inverse(viewProj), eye, ndc);

    REQUIRE_THAT(
      glm::length(ray.direction),
      WithinAbs(1.0f, GRADIENT_TOLERANCE)
    );
  }
}

TEST_CASE("a march reports the near surface of a sphere", "[raycast]") {
  const std::vector<Primitive> primitives{
    makeSphere(glm::vec3(0.0f, 0.0f, -5.0f), 1.0f, 4)
  };

  const Ray ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};

  const std::optional<RayHit> hit = dunya::field::raymarch(primitives, ray);

  REQUIRE(hit.has_value());
  REQUIRE_THAT(hit->travelled, WithinAbs(4.0f, MARCH_TOLERANCE));
  REQUIRE_THAT(hit->position.z, WithinAbs(-4.0f, MARCH_TOLERANCE));
  REQUIRE(hit->material == 4);
}

TEST_CASE("a march that meets nothing reports nothing", "[raycast]") {
  const std::vector<Primitive> primitives{
    makeSphere(glm::vec3(0.0f, 0.0f, -5.0f), 1.0f, 4)
  };

  const Ray ray{glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)};

  REQUIRE_FALSE(dunya::field::raymarch(primitives, ray).has_value());
}

TEST_CASE("a march gives up past its distance budget", "[raycast]") {
  const std::vector<Primitive> primitives{
    makeSphere(glm::vec3(0.0f, 0.0f, -50.0f), 1.0f, 4)
  };

  const Ray ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};

  dunya::field::MarchSettings shortBudget;
  shortBudget.maxDistance = 10.0f;

  REQUIRE_FALSE(
    dunya::field::raymarch(primitives, ray, shortBudget).has_value()
  );
  REQUIRE(dunya::field::raymarch(primitives, ray).has_value());
}

TEST_CASE("over-relaxation finds the same surface", "[raycast]") {
  // Over-relaxation is only allowed to reach the surface in fewer steps, never
  // to land somewhere else or miss. A blended blob and a carve make the
  // distance estimate conservative, which is exactly when relaxation oversteps.
  const std::vector<Primitive> primitives{
    makeSphere(glm::vec3(0.0f, 0.0f, -5.0f), 1.0f, 4),
    makeSphere(glm::vec3(0.9f, 0.0f, -5.0f), 0.7f, 5),
    makeSphere(glm::vec3(0.0f, 0.0f, -4.2f), 0.3f, 6)
  };

  dunya::field::MarchSettings unrelaxed;
  unrelaxed.omega = 1.0f;

  for (float y = -0.6f; y <= 0.6f; y += 0.1f) {
    const Ray ray{glm::vec3(0.0f), glm::normalize(glm::vec3(0.0f, y, -1.0f))};

    const std::optional<RayHit> relaxed =
      dunya::field::raymarch(primitives, ray);
    const std::optional<RayHit> plain =
      dunya::field::raymarch(primitives, ray, unrelaxed);

    REQUIRE(relaxed.has_value() == plain.has_value());

    if (relaxed.has_value()) {
      REQUIRE_THAT(
        relaxed->travelled,
        WithinAbs(plain->travelled, MARCH_TOLERANCE)
      );
    }
  }
}

TEST_CASE("a march starting inside geometry walks out to it", "[raycast]") {
  // Starting inside solid, the surface is the boundary ahead, not the point
  // the ray began at. The shader does the same, which is what lets the camera
  // fly inside a shape and see its interior - and what makes a click from in
  // there land where the image says it should.
  const std::vector<Primitive> primitives{makeSphere(glm::vec3(0.0f), 1.0f, 7)};

  const Ray ray{glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};

  const std::optional<RayHit> hit = dunya::field::raymarch(primitives, ray);

  REQUIRE(hit.has_value());
  REQUIRE_THAT(hit->travelled, WithinAbs(1.0f, MARCH_TOLERANCE));
  REQUIRE_THAT(hit->position.x, WithinAbs(1.0f, MARCH_TOLERANCE));
  REQUIRE(hit->material == 7);
}

TEST_CASE("a ray through a box reports entry and exit", "[raycast]") {
  // Axis-aligned, so the slab arithmetic is exact and the expected distances
  // can be stated rather than bracketed.
  const Aabb box{glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(3.0f, 1.0f, 1.0f)};
  const Ray ray{glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};

  const std::optional<std::pair<float, float>> t =
    dunya::field::intersect(box, ray);

  REQUIRE(t.has_value());
  REQUIRE_THAT(t->first, WithinAbs(1.0f, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(t->second, WithinAbs(3.0f, ANALYTIC_TOLERANCE));
}

TEST_CASE("a ray that misses a box reports nothing", "[raycast]") {
  const Aabb box{glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(3.0f, 1.0f, 1.0f)};

  // Beside and parallel: the y slab is degenerate and the origin sits outside
  // it, which is the divide-by-zero branch.
  const Ray parallel{glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};
  REQUIRE_FALSE(dunya::field::intersect(box, parallel).has_value());

  // Beside and slanted: both slabs are real but their intervals never overlap.
  const Ray slanted{
    glm::vec3(0.0f, 5.0f, 0.0f),
    glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f))
  };
  REQUIRE_FALSE(dunya::field::intersect(box, slanted).has_value());
}

TEST_CASE("a box behind the ray is not an intersection", "[raycast]") {
  const Aabb box{glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(3.0f, 1.0f, 1.0f)};

  // The ray's line crosses the box, but only at negative distances.
  const Ray ray{glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};

  REQUIRE_FALSE(dunya::field::intersect(box, ray).has_value());
}

TEST_CASE("a ray starting inside a box enters at distance zero", "[raycast]") {
  // The geometric entry is behind the origin; the caller must get zero, not a
  // negative distance it would march backwards to.
  const Aabb box{glm::vec3(1.0f, -1.0f, -1.0f), glm::vec3(3.0f, 1.0f, 1.0f)};
  const Ray ray{glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};

  const std::optional<std::pair<float, float>> t =
    dunya::field::intersect(box, ray);

  REQUIRE(t.has_value());
  REQUIRE_THAT(t->first, WithinAbs(0.0f, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(t->second, WithinAbs(1.0f, ANALYTIC_TOLERANCE));
}
