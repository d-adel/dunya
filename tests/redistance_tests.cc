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

void flatten(SampledField& field, float factor) {
  const float keep =
    1.5f * std::min({field.voxelSize.x, field.voxelSize.y, field.voxelSize.z});

  for (float& value : field.distances) {
    if (std::abs(value) > keep) {
      value *= factor;
    }
  }

  dunya::field::write(field, whole(field), field.distances, field.materials);
}

}

TEST_CASE("the Godunov update solves its own equation", "[redistance]") {
  const glm::vec3 unit(1.0f);

  REQUIRE_THAT(
    dunya::field::godunov(0.0f, FAR, FAR, unit),
    WithinAbs(1.0f, 1e-6f)
  );

  REQUIRE_THAT(
    dunya::field::godunov(0.0f, 0.0f, FAR, unit),
    WithinAbs(1.0f / std::sqrt(2.0f), 1e-6f)
  );

  REQUIRE_THAT(
    dunya::field::godunov(0.0f, 0.0f, 0.0f, unit),
    WithinAbs(1.0f / std::sqrt(3.0f), 1e-6f)
  );

  REQUIRE_THAT(
    dunya::field::godunov(0.0f, FAR, FAR, glm::vec3(0.5f, 1.0f, 1.0f)),
    WithinAbs(0.5f, 1e-6f)
  );

  REQUIRE_THAT(
    dunya::field::godunov(0.0f, 0.0f, FAR, glm::vec3(0.5f, 1.0f, 1.0f)),
    WithinAbs(1.0f / std::sqrt(5.0f), 1e-6f)
  );

  REQUIRE(dunya::field::godunov(FAR, FAR, FAR, unit) >= FAR);

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

  REQUIRE(worstBefore > 2.0f * voxel);

  const float settled = dunya::field::redistance(field, whole(field));

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

  REQUIRE(worstAfter < 0.5f * voxel);

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

  REQUIRE(worst < 0.25f * field.voxelSize.y);
}

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

  REQUIRE(worst == 0.0f);
}
