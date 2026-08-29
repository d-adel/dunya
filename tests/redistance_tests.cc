#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/field/analytic/analytic.h>
#include <dunya/field/field.h>
#include <dunya/field/redistance/redistance.h>
#include <dunya/field/sampled/sampled.h>

#include <glm/glm.hpp>

#include <cmath>
#include <limits>
#include <vector>

using Catch::Matchers::WithinAbs;
using dunya::field::Primitive;
using dunya::field::SampledField;

namespace {

constexpr float FAR = std::numeric_limits<float>::max();

dunya::field::SampleBox whole(const SampledField& field) {
  return {glm::uvec3(0u), field.resolution};
}

std::vector<uint8_t> damagedEverywhere(const SampledField& field) {
  return std::vector<uint8_t>(
    static_cast<size_t>(field.resolution.x) * field.resolution.y
      * field.resolution.z,
    1u
  );
}

// Scales values away from the surface, leaving the layer either side of it
// alone. That is the shape of the damage a CSG fold does - exact where the two
// surfaces meet, progressively wrong further out - and it is the shape the
// sweep can state a right answer for. Scaling the seed layer as well would
// move the sub-voxel crossing, which is damage nothing produces and the sweep
// deliberately will not repair.
void flatten(SampledField& field, float factor) {
  // The finest axis, not the coarsest. On the slab the coarsest is 0.165 while
  // the whole field only reaches 0.15, so a coarsest-axis threshold left every
  // one of 35,937 values untouched and the test measured reconstruction of an
  // exact field rather than repair of a damaged one.
  const float keep =
    1.5f * std::min({field.voxelSize.x, field.voxelSize.y, field.voxelSize.z});

  for (float& value : field.distances) {
    if (std::abs(value) > keep) {
      value *= factor;
    }
  }

  dunya::field::write(field, whole(field), field.distances, field.materials);
}

}  // namespace

TEST_CASE("the Godunov update solves its own equation", "[redistance]") {
  const glm::vec3 unit(1.0f);

  // One term: the far neighbours contribute nothing, so it is a step of h.
  REQUIRE_THAT(
    dunya::field::godunov(0.0f, FAR, FAR, unit),
    WithinAbs(1.0f, 1e-6f)
  );

  // Two terms at the same value: x^2 + x^2 = 1.
  REQUIRE_THAT(
    dunya::field::godunov(0.0f, 0.0f, FAR, unit),
    WithinAbs(1.0f / std::sqrt(2.0f), 1e-6f)
  );

  // Three: 3x^2 = 1.
  REQUIRE_THAT(
    dunya::field::godunov(0.0f, 0.0f, 0.0f, unit),
    WithinAbs(1.0f / std::sqrt(3.0f), 1e-6f)
  );

  // Anisotropic, one term: the step is that axis' own spacing, not some
  // average of the three.
  REQUIRE_THAT(
    dunya::field::godunov(0.0f, FAR, FAR, glm::vec3(0.5f, 1.0f, 1.0f)),
    WithinAbs(0.5f, 1e-6f)
  );

  // Anisotropic, two terms: x^2/0.25 + x^2/1 = 1, so x = 1/sqrt(5).
  REQUIRE_THAT(
    dunya::field::godunov(0.0f, 0.0f, FAR, glm::vec3(0.5f, 1.0f, 1.0f)),
    WithinAbs(1.0f / std::sqrt(5.0f), 1e-6f)
  );

  // Nothing known anywhere: there is no answer, and it must not invent one.
  REQUIRE(dunya::field::godunov(FAR, FAR, FAR, unit) >= FAR);

  // A neighbour further than one step away drops out of the solution rather
  // than dragging it up: this is what "upwind" buys.
  REQUIRE_THAT(
    dunya::field::godunov(0.0f, 5.0f, 5.0f, unit),
    WithinAbs(1.0f, 1e-6f)
  );
}

TEST_CASE("a flattened sphere is repaired inside the band", "[redistance]") {
  const std::vector<Primitive> primitives{
    dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, 3u)
  };

  SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(65u)
  );

  const float voxel = 4.0f / 64.0f;

  flatten(field, 0.3f);

  // Before: the gradient is a third of what it should be everywhere.
  float worstBefore = 0.0f;

  for (uint32_t z = 8u; z < 57u; ++z) {
    for (uint32_t y = 8u; y < 57u; ++y) {
      for (uint32_t x = 8u; x < 57u; ++x) {
        const glm::vec3 point =
          field.origin + field.voxelSize * glm::vec3(x, y, z);

        const float truth = glm::length(point) - 1.0f;

        if (std::abs(truth) > 0.4f) {
          continue;
        }

        const uint32_t index = x + 65u * (y + 65u * z);

        worstBefore =
          std::max(worstBefore, std::abs(field.distances[index] - truth));
      }
    }
  }

  // A third of the true value inside a band of 0.4 is up to 0.28 of damage.
  REQUIRE(worstBefore > 2.0f * voxel);

  const float settled = dunya::field::redistance(field, whole(field));

  // Only the band, which is the whole of what D7 promises: further out the
  // box's own shell is a Dirichlet condition and still carries the damage.
  float worstAfter = 0.0f;

  for (uint32_t z = 8u; z < 57u; ++z) {
    for (uint32_t y = 8u; y < 57u; ++y) {
      for (uint32_t x = 8u; x < 57u; ++x) {
        const glm::vec3 point =
          field.origin + field.voxelSize * glm::vec3(x, y, z);

        const float truth = glm::length(point) - 1.0f;

        if (std::abs(truth) > 0.4f) {
          continue;
        }

        const uint32_t index = x + 65u * (y + 65u * z);

        worstAfter =
          std::max(worstAfter, std::abs(field.distances[index] - truth));
      }
    }
  }

  // Measured at 0.24 of a voxel. Seeding from a whole voxel instead of
  // freezing the arrival value gives exactly 1.0, which is what this
  // threshold has to sit between.
  REQUIRE(worstAfter < 0.5f * voxel);

  // Eight sweeps do not settle a whole-grid repair - the last one still moves
  // values by 0.127 here, because a front has to cross 65 samples. A dent box
  // is a fraction of that and does settle. Pinned so the number is on record
  // rather than assumed away.
  REQUIRE(settled > 0.0f);
  REQUIRE(settled < 0.2f);

  const float longer = dunya::field::redistance(
    field,
    whole(field),
    damagedEverywhere(field),
    64u
  );

  REQUIRE(longer < settled);
}

TEST_CASE("the repair uses each axis' own spacing", "[redistance]") {
  // Ten to one between the axes. A sweep that collapses the three spacings
  // into one cannot be right on both at once, which is what this catches.
  const std::vector<Primitive> primitives{
    dunya::field::makeBox(glm::vec3(0.0f), glm::vec3(4.0f, 0.1f, 4.0f))
  };

  SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f, -0.25f, -2.0f),
    glm::vec3(2.0f, 0.25f, 2.0f),
    glm::uvec3(33u)
  );

  REQUIRE(field.voxelSize.x > 7.0f * field.voxelSize.y);

  flatten(field, 0.4f);

  dunya::field::redistance(field, whole(field));

  // Down the middle of the slab, where the nearest surface is the face above
  // or below and the answer is known in closed form.
  float worst = 0.0f;

  for (uint32_t y = 4u; y < 29u; ++y) {
    const glm::vec3 point =
      field.origin + field.voxelSize * glm::vec3(16u, y, 16u);

    const float truth = std::abs(point.y) - 0.1f;

    if (std::abs(truth) > 0.06f) {
      continue;
    }

    const uint32_t index = 16u + 33u * (y + 33u * 16u);

    worst = std::max(worst, std::abs(field.distances[index] - truth));
  }

  // A plane is linear, so the seed interpolation is exact and this is machine
  // zero when the sweep is right. Collapsing the three spacings gives 0.33,
  // and a whole-voxel seed gives 0.6 of a voxel.
  REQUIRE(worst < 0.25f * field.voxelSize.y);
}

// Where the field changes sign along +y, to sub-voxel precision. Lattice signs
// are not the zero set: the crossing between two points is the ratio of their
// two values, so a repair can move the surface without flipping any sign.
float crossingAlongY(const SampledField& field, float from, float until) {
  float low = from;
  float high = until;

  for (uint32_t step = 0; step != 60u; ++step) {
    const float middle = 0.5f * (low + high);

    if (dunya::field::distance(field, glm::vec3(0.05f, middle, 0.05f)) < 0.0f) {
      low = middle;
    } else {
      high = middle;
    }
  }

  return 0.5f * (low + high);
}

TEST_CASE("the repair leaves the zero set where it was", "[redistance]") {
  const std::vector<Primitive> primitives{
    dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, 3u)
  };

  SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  const std::vector<float> before = field.distances;
  const float crossingBefore = crossingAlongY(field, 0.0f, 1.6f);

  flatten(field, 0.6f);
  dunya::field::redistance(field, whole(field));

  for (size_t i = 0; i < before.size(); ++i) {
    REQUIRE((before[i] < 0.0f) == (field.distances[i] < 0.0f));
  }

  // The assertion that actually pins it. Signs alone cannot fail here: the
  // repair builds its output from the input signs, so only an exact zero could
  // differ. Rewriting a seed value moves this by a fraction of a voxel while
  // every sign stays put - which is the bug that made the craters lumpy.
  REQUIRE_THAT(
    crossingAlongY(field, 0.0f, 1.6f),
    WithinAbs(crossingBefore, 1.0e-6f)
  );
}

TEST_CASE("repairing a repaired field changes almost nothing", "[redistance]") {
  const std::vector<Primitive> primitives{
    dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, 3u)
  };

  SampledField field = dunya::field::bake(
    primitives,
    glm::vec3(-2.0f),
    glm::vec3(2.0f),
    glm::uvec3(33u)
  );

  dunya::field::redistance(field, whole(field));

  const std::vector<float> once = field.distances;

  dunya::field::redistance(field, whole(field));

  float worst = 0.0f;

  for (size_t i = 0; i < once.size(); ++i) {
    worst = std::max(worst, std::abs(once[i] - field.distances[i]));
  }

  // Bit-identical, not merely close: the seeds are frozen at the values the
  // first pass wrote and the sweep is deterministic, so a second pass has
  // nothing to change. A tolerance here would pass real non-idempotency.
  REQUIRE(worst == 0.0f);
}
