#pragma once

#include <dunya/field/capability/distancefield.h>
#include <dunya/field/capability/gradientquery.h>
#include <dunya/field/capability/materialquery.h>
#include <dunya/field/capability/stepbound.h>
#include <dunya/field/field.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace dunya::field {

/* A distance field baked onto a regular lattice.
 *
 * Values sit at lattice points, not voxel centres, so a resolution of N spans
 * N-1 cells along each axis and a sample taken exactly at a lattice point
 * returns the baked value untouched.
 *
 * The grid is bounded and the field it came from may not be, so sampling
 * outside returns the distance to the grid's box. That is a valid lower bound
 * on the distance to anything inside it, which is what keeps a ray marching
 * toward the grid rather than stepping past it.
 */

// Cells per brick on each axis. A dent invalidates the bricks it touches
// instead of the whole grid, and the maxima below stay tight. The shaders
// index the same table, so the number comes from CMake.
inline constexpr uint32_t BRICK_CELLS = DUNYA_BRICK_CELLS;

struct SampledField {
  glm::vec3 origin{0.0f};
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};

  std::vector<float> distances;
  std::vector<uint32_t> materials;

  // A bound on the interpolant's own gradient, per brick and over all of them.
  // Derived from the stored values, so it survives edits with no source field.
  std::vector<float> brickLipschitz;
  float globalLipschitz = 0.0f;
};

// A block of lattice points, counted in samples rather than cells.
struct SampleBox {
  glm::uvec3 minimum{0u};
  glm::uvec3 extent{0u};
};

// The spacing a lattice of this resolution has across this box. The bake and
// the pass that re-places the grid after an edit both need it, and one of them
// writing the division itself is how the two drift apart.
glm::vec3 voxelSize(
  const glm::vec3& minimum,
  const glm::vec3& maximum,
  const glm::uvec3& resolution
);

SampledField bake(
  std::span<const Primitive> primitives,
  const glm::vec3& minimum,
  const glm::vec3& maximum,
  const glm::uvec3& resolution
);

// Trilinear between lattice points, and the distance to the grid's box outside
// it, which is a lower bound on anything the grid contains.
float distance(const SampledField& field, const glm::vec3& point);

uint32_t material(const SampledField& field, const glm::vec3& point);

// The interpolant's own derivative, not a difference of it: the same quantity
// the brick bounds cover, so a step and a normal cannot disagree.
glm::vec3 gradient(const SampledField& field, const glm::vec3& point);

// How far a ray at point may travel along a normalised direction without
// crossing the zero surface. Inside the grid the brick's bound gives the step
// and the brick's exit caps it, since the next brick may be steeper.
float stepBound(
  const SampledField& field,
  const glm::vec3& point,
  const glm::vec3& direction
);

// How far the baked values may sit from the field they came from, for a source
// with the given Lipschitz constant. Fidelity, not traversal safety.
float bakeError(const SampledField& field, float sourceLipschitz = 1.0f);

// The only way to change the lattice. It rebuilds the bricks the written
// samples participate in, which is why nothing else may write those arrays.
void write(
  SampledField& field,
  const SampleBox& box,
  std::span<const float> distances,
  std::span<const uint32_t> materials
);

static_assert(DistanceField<SampledField>);
static_assert(MaterialQueryable<SampledField>);
static_assert(GradientQueryable<SampledField>);
static_assert(StepBounded<SampledField>);

}  // namespace dunya::field
