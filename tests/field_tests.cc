#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>

#include "tolerances.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

using Catch::Matchers::WithinAbs;
using dunya::field::AnalyticSample;
using dunya::field::Primitive;

namespace {

constexpr uint32_t SPHERE = 0;
constexpr uint32_t BOX = 1;
constexpr uint32_t PLANE = 2;

constexpr uint32_t UNION = 0;
constexpr uint32_t SMOOTH_UNION = 1;
constexpr uint32_t INTERSECTION = 2;
constexpr uint32_t SUBTRACTION = 3;
constexpr uint32_t SMOOTH_SUBTRACTION = 4;

Primitive makePrimitive(
  uint32_t type,
  const glm::vec3& translation,
  const glm::vec4& shape,
  uint32_t material,
  uint32_t operation
) {
  Primitive primitive{};
  primitive.inverseModel =
    glm::inverse(glm::translate(glm::mat4(1.0f), translation));
  primitive.shape = shape;
  primitive.shapeConfig = glm::uvec4(type, material, operation, 0);

  return primitive;
}

Primitive makeTransformedPrimitive(
  uint32_t type,
  const glm::mat4& model,
  const glm::vec4& shape,
  uint32_t material,
  uint32_t operation
) {
  Primitive primitive{};
  primitive.inverseModel = glm::inverse(model);
  primitive.shape = shape;
  primitive.shapeConfig = glm::uvec4(type, material, operation, 0);

  return primitive;
}

float distanceAt(const std::vector<Primitive>& primitives, const glm::vec3& p) {
  return dunya::field::sample(primitives, p).distance;
}

}  // namespace

TEST_CASE("Primitive keeps the layout the shader indexes by", "[field]") {
  REQUIRE(sizeof(Primitive) == 112);
  REQUIRE(alignof(Primitive) == 16);
  REQUIRE(offsetof(Primitive, inverseModel) == 0);
  REQUIRE(offsetof(Primitive, shape) == 64);
  REQUIRE(offsetof(Primitive, shapeConfig) == 80);
  REQUIRE(offsetof(Primitive, bounds) == 96);
}

TEST_CASE("bounds culling does not change the field", "[field][bounds]") {
  // Two spheres, a smooth-union blob, a hard carve and a smooth carve, so
  // every skip condition is exercised, then the same scene with bounds filled
  // in. Culling is only ever allowed to save work, so every sample has to agree
  // exactly. The smooth carve is the one that catches a bound that forgot to
  // carry its blend radius: it starts rounding k before the cutter arrives, so
  // a bound sized to the bare radius would skip it while it still had work.
  std::vector<Primitive> unbounded{
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 3, UNION),
    makePrimitive(
      SPHERE,
      glm::vec3(1.4f, 0.0f, 0.0f),
      glm::vec4(0.8f, 0.0f, 0.0f, 0.5f),
      5,
      SMOOTH_UNION
    ),
    makePrimitive(
      SPHERE,
      glm::vec3(0.0f, 0.9f, 0.0f),
      glm::vec4(0.4f, 0, 0, 0),
      7,
      SUBTRACTION
    ),
    makePrimitive(
      SPHERE,
      glm::vec3(-0.6f, 0.0f, 0.5f),
      glm::vec4(0.45f, 0.0f, 0.0f, 0.3f),
      11,
      SMOOTH_SUBTRACTION
    ),
    makePrimitive(
      SPHERE,
      glm::vec3(6.0f, 6.0f, 6.0f),
      glm::vec4(0.5f, 0, 0, 0),
      9,
      UNION
    )
  };

  std::vector<Primitive> bounded = unbounded;
  for (Primitive& primitive : bounded) {
    dunya::field::updateBounds(primitive);
  }

  for (float x = -3.0f; x <= 3.0f; x += 0.25f) {
    for (float y = -3.0f; y <= 3.0f; y += 0.25f) {
      for (float z = -3.0f; z <= 3.0f; z += 0.5f) {
        const glm::vec3 point(x, y, z);

        const AnalyticSample plain = dunya::field::sample(unbounded, point);
        const AnalyticSample culled = dunya::field::sample(bounded, point);

        REQUIRE_THAT(
          culled.distance,
          WithinAbs(plain.distance, ANALYTIC_TOLERANCE)
        );
        REQUIRE(culled.material == plain.material);
      }
    }
  }
}

TEST_CASE("an empty field is far away and has material zero", "[field]") {
  const std::vector<Primitive> primitives;

  const AnalyticSample result =
    dunya::field::sample(primitives, glm::vec3(0.0f));

  REQUIRE(result.distance > 1e8f);
  REQUIRE(result.material == 0);
}

TEST_CASE("a sphere reports signed distance to its surface", "[field]") {
  const std::vector<Primitive> primitives{
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 0, UNION)
  };

  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(2.0f, 0.0f, 0.0f)),
    WithinAbs(1.0f, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(0.0f)),
    WithinAbs(-1.0f, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(0.0f, 0.0f, 1.0f)),
    WithinAbs(0.0f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("a box reports signed distance to its surface", "[field]") {
  const std::vector<Primitive> primitives{makePrimitive(
    BOX,
    glm::vec3(0.0f),
    glm::vec4(0.5f, 0.5f, 0.5f, 0.0f),
    0,
    UNION
  )};

  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(1.0f, 0.0f, 0.0f)),
    WithinAbs(0.5f, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(0.0f)),
    WithinAbs(-0.5f, ANALYTIC_TOLERANCE)
  );

  // Diagonally outside a corner the distance is the length of the overhang.
  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(1.0f, 1.0f, 0.5f)),
    WithinAbs(std::sqrt(0.5f), ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("a plane measures along its own up axis", "[field]") {
  const std::vector<Primitive> primitives{makePrimitive(
    PLANE,
    glm::vec3(0.0f, -2.0f, 0.0f),
    glm::vec4(0.0f),
    0,
    UNION
  )};

  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(0.0f)),
    WithinAbs(2.0f, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(0.0f, -3.0f, 0.0f)),
    WithinAbs(-1.0f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("a rigid transform carries the field with the shape", "[field]") {
  const std::vector<Primitive> primitives{makePrimitive(
    SPHERE,
    glm::vec3(5.0f, 0.0f, 0.0f),
    glm::vec4(1.0f, 0, 0, 0),
    0,
    UNION
  )};

  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(5.0f, 0.0f, 0.0f)),
    WithinAbs(-1.0f, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(7.0f, 0.0f, 0.0f)),
    WithinAbs(1.0f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("a rotation turns the field with the shape", "[field]") {
  const glm::vec4 longBox(1.0f, 0.2f, 0.2f, 0.0f);
  const glm::vec3 probe(0.0f, 0.0f, 0.9f);

  const std::vector<Primitive> unrotated{
    makeTransformedPrimitive(BOX, glm::mat4(1.0f), longBox, 0, UNION)
  };

  const std::vector<Primitive> rotated{makeTransformedPrimitive(
    BOX,
    glm::rotate(
      glm::mat4(1.0f),
      glm::radians(90.0f),
      glm::vec3(0.0f, 1.0f, 0.0f)
    ),
    longBox,
    0,
    UNION
  )};

  // The probe sits off the end of the box's long axis. Turning the box a
  // quarter turn about Y swings that axis onto Z and swallows the probe.
  REQUIRE_THAT(
    distanceAt(unrotated, probe),
    WithinAbs(0.7f, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    distanceAt(rotated, probe),
    WithinAbs(-0.1f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("union takes the nearer surface and its material", "[field][csg]") {
  const std::vector<Primitive> primitives{
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 3, UNION),
    makePrimitive(
      SPHERE,
      glm::vec3(4.0f, 0.0f, 0.0f),
      glm::vec4(1.0f, 0, 0, 0),
      8,
      UNION
    )
  };

  const AnalyticSample near =
    dunya::field::sample(primitives, glm::vec3(4.0f, 0.0f, 0.0f));

  REQUIRE_THAT(near.distance, WithinAbs(-1.0f, ANALYTIC_TOLERANCE));
  REQUIRE(near.material == 8);
}

TEST_CASE("intersection takes the farther surface", "[field][csg]") {
  const std::vector<Primitive> primitives{
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 3, UNION),
    makePrimitive(
      SPHERE,
      glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec4(1.0f, 0, 0, 0),
      8,
      INTERSECTION
    )
  };

  // Inside the first, further from the second: the second's distance wins.
  const AnalyticSample result =
    dunya::field::sample(primitives, glm::vec3(-0.5f, 0.0f, 0.0f));

  REQUIRE_THAT(result.distance, WithinAbs(0.5f, ANALYTIC_TOLERANCE));
  REQUIRE(result.material == 8);
}

TEST_CASE("subtraction keeps the accumulated material", "[field][csg]") {
  const std::vector<Primitive> primitives{
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 3, UNION),
    makePrimitive(
      SPHERE,
      glm::vec3(0.0f),
      glm::vec4(0.5f, 0, 0, 0),
      8,
      SUBTRACTION
    )
  };

  const AnalyticSample result =
    dunya::field::sample(primitives, glm::vec3(0.0f));

  REQUIRE_THAT(result.distance, WithinAbs(0.5f, ANALYTIC_TOLERANCE));
  REQUIRE(result.material == 3);
}

TEST_CASE(
  "smooth subtraction rounds where the hard one creases",
  "[field][csg]"
) {
  // A unit sphere with a smaller sphere cut out of it, k = 0.5. At radius 0.75
  // the accumulated distance is -0.25 and the cutter's is +0.25, so the two
  // arguments of the smooth max are equal: h is 0.5 and the blend adds
  // k*h*(1-h) = 0.125 to the hard answer. Hand-computed, the same way the
  // smooth union case is, because the whole point is that this agrees with the
  // shader's arithmetic and not merely with itself.
  const Primitive solid =
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 3, UNION);

  const std::vector<Primitive> hard{
    solid,
    makePrimitive(
      SPHERE,
      glm::vec3(0.0f),
      glm::vec4(0.5f, 0.0f, 0.0f, 0.5f),
      8,
      SUBTRACTION
    )
  };

  const std::vector<Primitive> smooth{
    solid,
    makePrimitive(
      SPHERE,
      glm::vec3(0.0f),
      glm::vec4(0.5f, 0.0f, 0.0f, 0.5f),
      8,
      SMOOTH_SUBTRACTION
    )
  };

  const glm::vec3 probe(0.75f, 0.0f, 0.0f);

  REQUIRE_THAT(distanceAt(hard, probe), WithinAbs(-0.25f, ANALYTIC_TOLERANCE));
  REQUIRE_THAT(
    distanceAt(smooth, probe),
    WithinAbs(-0.125f, ANALYTIC_TOLERANCE)
  );

  // Smooth max sits above hard max, so a smooth carve removes slightly more.
  // That is the one place it is less conservative than the hard op, and worth
  // pinning rather than rediscovering as a march artifact.
  REQUIRE(distanceAt(smooth, probe) > distanceAt(hard, probe));

  // A subtraction of either kind keeps what it cut into.
  REQUIRE(dunya::field::sample(smooth, probe).material == 3);
}

TEST_CASE("smooth union blends by the shader's smin", "[field][csg]") {
  const std::vector<Primitive> primitives{
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 3, UNION),
    makePrimitive(
      SPHERE,
      glm::vec3(1.5f, 0.0f, 0.0f),
      glm::vec4(1.0f, 0.0f, 0.0f, 0.5f),
      8,
      SMOOTH_UNION
    )
  };

  // Both distances are -0.25 at the midpoint, so h is 0.5 and the blend
  // subtracts k*h*(1-h) = 0.125 from it.
  REQUIRE_THAT(
    distanceAt(primitives, glm::vec3(0.75f, 0.0f, 0.0f)),
    WithinAbs(-0.375f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("the fold is order dependent", "[field][csg]") {
  const Primitive big =
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 3, UNION);
  const Primitive carve = makePrimitive(
    SPHERE,
    glm::vec3(0.0f),
    glm::vec4(0.5f, 0, 0, 0),
    8,
    SUBTRACTION
  );
  const Primitive added = makePrimitive(
    SPHERE,
    glm::vec3(0.3f, 0.0f, 0.0f),
    glm::vec4(1.0f, 0, 0, 0),
    9,
    UNION
  );

  const std::vector<Primitive> carveThenAdd{big, carve, added};
  const std::vector<Primitive> addThenCarve{big, added, carve};

  // The subtraction applies to everything folded before it and nothing after,
  // so the same three primitives describe two different fields.
  REQUIRE_THAT(
    distanceAt(carveThenAdd, glm::vec3(0.0f)),
    WithinAbs(-0.7f, ANALYTIC_TOLERANCE)
  );
  REQUIRE_THAT(
    distanceAt(addThenCarve, glm::vec3(0.0f)),
    WithinAbs(0.5f, ANALYTIC_TOLERANCE)
  );
}

TEST_CASE("the gradient has unit length away from surfaces", "[field]") {
  const std::vector<Primitive> primitives{
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 0, UNION)
  };

  const glm::vec3 g =
    dunya::field::gradient(primitives, glm::vec3(1.5f, 1.5f, 1.5f));

  REQUIRE_THAT(glm::length(g), WithinAbs(1.0f, GRADIENT_TOLERANCE));
}

TEST_CASE("the normal points away from a sphere's centre", "[field]") {
  const std::vector<Primitive> primitives{
    makePrimitive(SPHERE, glm::vec3(0.0f), glm::vec4(1.0f, 0, 0, 0), 0, UNION)
  };

  const glm::vec3 n =
    dunya::field::normal(primitives, glm::vec3(3.0f, 0.0f, 0.0f));

  REQUIRE_THAT(n.x, WithinAbs(1.0f, GRADIENT_TOLERANCE));
  REQUIRE_THAT(n.y, WithinAbs(0.0f, GRADIENT_TOLERANCE));
  REQUIRE_THAT(n.z, WithinAbs(0.0f, GRADIENT_TOLERANCE));
}
