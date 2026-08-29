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

// A distance field on a regular lattice, values at lattice points, so
// resolution N spans N-1 cells. Sampling outside returns the distance to the
// grid's box, a lower bound that keeps a ray marching toward it.

// Cells per brick on each axis, so a dent invalidates the bricks it touches
// instead of the whole grid. The shaders index the same table, so the number
// comes from CMake.
inline constexpr uint32_t BRICK_CELLS = DUNYA_BRICK_CELLS;

struct SampledField {
  glm::vec3 origin{0.0f};
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};

  std::vector<float> distances;
  std::vector<uint8_t> materials;

  // A bound on the interpolant's own gradient, per brick and over all of them.
  // Derived from the stored values, so it survives edits with no source field.
  std::vector<float> brickLipschitz;
  float globalLipschitz = 0.0f;

  // The value range per brick, over the same haloed cells. The bound above
  // describes the gradient, so it cannot say whether a brick holds the surface.
  std::vector<float> brickMinimum;
  std::vector<float> brickMaximum;
};

// A block of lattice points, counted in samples rather than cells.
struct SampleBox {
  glm::uvec3 minimum{0u};
  glm::uvec3 extent{0u};
};

// The smallest box holding both. An empty box contributes nothing, so a frame
// can fold every dent it made into one upload without a first-time special
// case. Deliberately a bounding box rather than a list: it can carry voxels
// that did not change, and it is one copy instead of forty.
SampleBox merge(const SampleBox& first, const SampleBox& second);

// What a write changed. The brick range is half-open, and wider than the
// samples: a cell reads the eight points around it and a brick's bound reads
// one cell past its own wall, so a written point reaches a brick either side.
// Everything derived from a change - the upload, the mass integral, the
// contact seeds - reads this instead of working the range out again.
struct WriteReport {
  SampleBox samples;
  glm::uvec3 brickBegin{0u};
  glm::uvec3 brickEnd{0u};
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

// Distance and a unit normal from one walk, meaningful outside the grid as
// well as inside it. Deliberately not distance(): that one under-reports
// outside so a march cannot overshoot, and a contact needs the opposite.
struct FieldProbe {
  float distance = 0.0f;
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

FieldProbe probe(const SampledField& field, const glm::vec3& point);
// How far a ray at point may travel along a normalised direction without
// crossing the zero surface. Inside the grid the brick's bound gives the step
// and the brick's exit caps it, since the next brick may be steeper.
float stepBound(
  const SampledField& field,
  const glm::vec3& point,
  const glm::vec3& direction
);

// How many bricks the grid holds on each axis. A contact query walks these,
// so the count cannot stay private to the bake.
glm::uvec3 brickCounts(const SampledField& field);
// Whether a brick's value range straddles zero, which is the only cheap way to
// ask if it holds any surface. Contact generation starts from this.
bool brickHoldsSurface(const SampledField& field, uint32_t brick);

// How far the baked values may sit from the field they came from, for a source
// with the given Lipschitz constant. Fidelity, not traversal safety.
float bakeError(const SampledField& field, float sourceLipschitz = 1.0f);

// The only way to change the lattice. It rebuilds the bricks the written
// samples participate in, which is why nothing else may write those arrays.
WriteReport write(
  SampledField& field,
  const SampleBox& box,
  std::span<const float> distances,
  std::span<const uint8_t> materials
);

static_assert(DistanceField<SampledField>);
static_assert(MaterialQueryable<SampledField>);
static_assert(GradientQueryable<SampledField>);
static_assert(StepBounded<SampledField>);

}  // namespace dunya::field
