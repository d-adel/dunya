#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <dunya/core/config/config.h>
#include <dunya/field/analytic/analytic.h>
#include <dunya/field/deform/deform.h>
#include <dunya/field/field.h>
#include <dunya/field/sampled/sampled.h>

#include <glm/glm.hpp>

#include <optional>
#include <vector>

using Catch::Matchers::WithinAbs;
using dunya::field::Primitive;
using dunya::field::SampledField;

namespace {

constexpr float SPAN = 2.0f;
constexpr uint32_t RESOLUTION = 65u;
constexpr float VOXEL = 2.0f * SPAN / static_cast<float>(RESOLUTION - 1u);

// A subtracting sphere: the carve half of the general operation, which is
// what the dent tests were about.
Primitive carver(const glm::vec3& centre, float radius) {
  Primitive cutter = dunya::field::makeSphere(
    centre,
    radius,
    7u,
    dunya::core::FIELD_OP_SUBTRACTION
  );

  dunya::field::updateBounds(cutter);

  return cutter;
}

SampledField unitSphere() {
  const std::vector<Primitive> primitives{
    dunya::field::makeSphere(glm::vec3(0.0f), 1.0f, 3u)
  };

  return dunya::field::bake(
    primitives,
    glm::vec3(-SPAN),
    glm::vec3(SPAN),
    glm::uvec3(RESOLUTION)
  );
}

// Where the field changes sign along +x, found by bisection on the samples so
// the answer is the surface the marcher would actually meet.
std::optional<float> crossingAlongX(const SampledField& field, float until) {
  float previous = dunya::field::distance(field, glm::vec3(0.0f));

  for (uint32_t step = 1u; step <= 4096u; ++step) {
    const float x = until * static_cast<float>(step) / 4096.0f;
    const float here = dunya::field::distance(field, glm::vec3(x, 0.0f, 0.0f));

    if (previous < 0.0f && here >= 0.0f) {
      return x;
    }

    previous = here;
  }

  return std::nullopt;
}

}  // namespace

TEST_CASE(
  "a subtraction moves the surface to the cutter's near wall",
  "[deform]"
) {
  SampledField field = unitSphere();

  const std::optional<float> before = crossingAlongX(field, 1.5f);

  REQUIRE(before.has_value());
  REQUIRE_THAT(*before, WithinAbs(1.0f, VOXEL));

  // Centred on the sphere's +x pole, so the solid now ends at 1 - 0.35 and
  // there is nothing beyond it: the cutter's far wall is outside the sphere.
  dunya::field::deform(field, carver(glm::vec3(1.0f, 0.0f, 0.0f), 0.35f));

  const std::optional<float> after = crossingAlongX(field, 1.5f);

  REQUIRE(after.has_value());
  REQUIRE_THAT(*after, WithinAbs(0.65f, VOXEL));
}

TEST_CASE(
  "a subtraction agrees with max(old, -cutter) inside the band",
  "[deform]"
) {
  SampledField field = unitSphere();

  const SampledField before = field;

  const glm::vec3 centre(1.0f, 0.0f, 0.0f);
  const float radius = 0.35f;

  dunya::field::deform(field, carver(centre, radius));

  const glm::uvec3 resolution = field.resolution;
  const float margin =
    static_cast<float>(dunya::field::DEFORM_BAND_VOXELS) * VOXEL;

  const dunya::field::SampleBox box = dunya::field::affectedBox(
    field,
    carver(centre, radius),
    dunya::field::DEFORM_BAND_VOXELS
  );

  const glm::uvec3 beyond = box.minimum + box.extent;

  for (uint32_t z = 0u; z < resolution.z; ++z) {
    for (uint32_t y = 0u; y < resolution.y; ++y) {
      for (uint32_t x = 0u; x < resolution.x; ++x) {
        const uint32_t index = x + resolution.x * (y + resolution.y * z);

        const glm::vec3 point =
          field.origin + field.voxelSize * glm::vec3(x, y, z);

        const float cutter = glm::length(point - centre) - radius;
        const float want = std::max(before.distances[index], -cutter);

        const bool written = x >= box.minimum.x && x < beyond.x
                             && y >= box.minimum.y && y < beyond.y
                             && z >= box.minimum.z && z < beyond.z;

        // Inside the box the operation has to be exact. Outside it, skipping
        // the write is either a no-op or only leaves values already deeper
        // than the band - which is precisely what D7 declines to promise.
        if (written) {
          REQUIRE(field.distances[index] == want);
        } else {
          REQUIRE(field.distances[index] == before.distances[index]);
          REQUIRE((want == before.distances[index] || want <= -margin));
        }
      }
    }
  }
}

TEST_CASE("a deformation that misses the grid writes nothing", "[deform]") {
  SampledField field = unitSphere();

  const SampledField before = field;

  const dunya::field::WriteReport report =
    dunya::field::deform(field, carver(glm::vec3(100.0f, 0.0f, 0.0f), 0.35f));

  REQUIRE(report.samples.extent.x == 0u);
  REQUIRE(report.brickBegin.x == report.brickEnd.x);
  REQUIRE(report.brickBegin.y == report.brickEnd.y);
  REQUIRE(report.brickBegin.z == report.brickEnd.z);

  REQUIRE(field.distances == before.distances);
  REQUIRE(field.materials == before.materials);
}

TEST_CASE("a deformation over the lattice edge clamps", "[deform]") {
  SampledField field = unitSphere();

  // Centred on the grid's own corner, so most of the cutter is outside it.
  REQUIRE_NOTHROW(dunya::field::deform(field, carver(glm::vec3(-SPAN), 0.5f)));

  const dunya::field::SampleBox box =
    dunya::field::affectedBox(field, carver(glm::vec3(-SPAN), 0.5f), 0u);

  REQUIRE(box.minimum.x == 0u);
  REQUIRE(box.minimum.y == 0u);
  REQUIRE(box.minimum.z == 0u);
  REQUIRE(box.extent.x < RESOLUTION);
}

TEST_CASE("subtraction exposes material, it does not paint it", "[deform]") {
  SampledField field = unitSphere();

  const SampledField before = field;

  // The cutter carries material 7 and the sphere is material 3. A hard
  // subtraction keeps the accumulator's material, because carving reveals what
  // was already there - which is what the analytic fold does, and the two
  // representations have to agree about it.
  dunya::field::deform(field, carver(glm::vec3(1.0f, 0.0f, 0.0f), 0.35f));

  uint32_t moved = 0u;

  for (size_t i = 0; i < field.distances.size(); ++i) {
    REQUIRE(field.materials[i] == before.materials[i]);

    if (field.distances[i] != before.distances[i]) {
      ++moved;
    }
  }

  REQUIRE(moved > 0u);
}

TEST_CASE("a union brings its own material with it", "[deform]") {
  SampledField field = unitSphere();

  const SampledField before = field;

  Primitive blob = dunya::field::makeSphere(
    glm::vec3(1.0f, 0.0f, 0.0f),
    0.35f,
    7u,
    dunya::core::FIELD_OP_UNION
  );

  dunya::field::updateBounds(blob);
  dunya::field::deform(field, blob);

  uint32_t painted = 0u;

  for (size_t i = 0; i < field.materials.size(); ++i) {
    if (field.materials[i] != before.materials[i]) {
      REQUIRE(field.materials[i] == 7u);
      ++painted;
    }
  }

  // Where, not merely how many. A fold that took the blob material over its
  // whole affected box would paint every one of the 24,389 samples in it and
  // still satisfy "painted > 0" with every value equal to 7.
  REQUIRE(dunya::field::material(field, glm::vec3(1.2f, 0.0f, 0.0f)) == 7u);
  REQUIRE(dunya::field::material(field, glm::vec3(0.0f)) == 3u);

  REQUIRE(painted > 0u);
  REQUIRE(painted < 15000u);
}

TEST_CASE("a union welds material on, at the far wall", "[deform]") {
  SampledField field = unitSphere();

  const std::optional<float> before = crossingAlongX(field, 2.5f);

  REQUIRE(before.has_value());
  REQUIRE_THAT(*before, WithinAbs(1.0f, VOXEL));

  Primitive blob = dunya::field::makeSphere(
    glm::vec3(1.0f, 0.0f, 0.0f),
    0.35f,
    7u,
    dunya::core::FIELD_OP_UNION
  );

  dunya::field::updateBounds(blob);
  dunya::field::deform(field, blob);

  // The same sphere, the other operation: the surface now ends at the blob's
  // far wall rather than at its near one.
  const std::optional<float> after = crossingAlongX(field, 2.5f);

  REQUIRE(after.has_value());
  REQUIRE_THAT(*after, WithinAbs(1.35f, VOXEL));
}

TEST_CASE("an operation only the general form can express", "[deform]") {
  SampledField field = unitSphere();

  // A box, not a sphere, and smooth-subtracted rather than hard: neither the
  // shape nor the operation was reachable when this took a centre and radius.
  Primitive chisel = dunya::field::makeBox(
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.4f),
    0.0f,
    glm::vec3(0.0f, 1.0f, 0.0f),
    7u,
    dunya::core::FIELD_OP_SMOOTH_SUBTRACTION,
    0.1f
  );

  dunya::field::updateBounds(chisel);

  const dunya::field::WriteReport report = dunya::field::deform(field, chisel);

  REQUIRE(report.samples.extent.x > 0u);
  REQUIRE(report.brickEnd.x > report.brickBegin.x);

  const std::optional<float> after = crossingAlongX(field, 1.5f);

  // The box reaches 0.4 from its centre at x = 1, so the solid ends at 0.6.
  // "less than 1" would pass a change of one part in eight thousand.
  REQUIRE(after.has_value());
  REQUIRE_THAT(*after, WithinAbs(0.6f, VOXEL));
}
