#pragma once

#include <cstdint>

namespace dunya::core {

using ObjectId = uint32_t;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// The shaders declare these too, so the numbers come from CMake, which is the
// only place that reaches both compilers.
constexpr uint32_t MAX_TEXTURES = DUNYA_MAX_TEXTURES;
constexpr uint32_t MAX_SAMPLERS = DUNYA_MAX_SAMPLERS;
constexpr uint32_t MAX_MATERIALS = DUNYA_MAX_MATERIALS;
constexpr uint32_t MAX_FIELD_OBJECTS = DUNYA_MAX_FIELD_OBJECTS;
constexpr uint32_t MAX_FIELD_PRIMITIVES = DUNYA_MAX_FIELD_PRIMITIVES;
constexpr size_t MAX_PRIMITIVE_POOL =
  static_cast<size_t>(MAX_FIELD_OBJECTS)
  * static_cast<size_t>(MAX_FIELD_PRIMITIVES);

constexpr uint32_t TEXTURE_WHITE = 0;
constexpr uint32_t TEXTURE_FLAT_NORMAL = 1;
constexpr uint32_t TEXTURE_BLACK = 2;
constexpr uint32_t RESERVED_TEXTURES = 3;

// Shared with the shaders, so the order Scene creates them in is one number.
constexpr uint32_t SAMPLER_LINEAR_REPEAT = DUNYA_SAMPLER_LINEAR_REPEAT;
constexpr uint32_t SAMPLER_LINEAR_CLAMP = DUNYA_SAMPLER_LINEAR_CLAMP;
constexpr uint32_t SAMPLER_NEAREST_CLAMP = DUNYA_SAMPLER_NEAREST_CLAMP;

// Which representation the field pass evaluates, as the shader reads it.
constexpr uint32_t FIELD_ANALYTIC = 0;
constexpr uint32_t FIELD_SAMPLED = 1;

// Operation ids as the field shader's shapeConfig.z reads them.
constexpr uint32_t FIELD_OP_UNION = 0;
constexpr uint32_t FIELD_OP_SMOOTH_UNION = 1;
constexpr uint32_t FIELD_OP_INTERSECTION = 2;
constexpr uint32_t FIELD_OP_SUBTRACTION = 3;

// Its own id rather than a blend radius on the hard one, which is how smooth
// union already relates to union. It also keeps the degenerate case out of the
// code entirely: smin at k = 0 is min by way of a division by zero, and 0/0
// where the two arguments are equal, so "zero means hard" would put a
// parameter exactly on a singularity (idiom 30).
constexpr uint32_t FIELD_OP_SMOOTH_SUBTRACTION = 4;

// Which family an operation belongs to, asked in one place. Callers care about
// the direction material moves, not about an id, and every downstream `== 3`
// is a site that a fifth operation would silently walk past (idiom 24).
constexpr bool fieldOpRemovesMaterial(uint32_t operation) {
  return operation == FIELD_OP_SUBTRACTION
         || operation == FIELD_OP_SMOOTH_SUBTRACTION;
}

constexpr float EDIT_RADIUS = 0.35f;

/* How far one click moves the surface, which is not the same question as how
 * wide a bite is.
 *
 * Welding the two forces the advance to a full radius, and that is the worst
 * stamp spacing there is: the union of equal spheres spaced s apart has a wall
 * oscillating between sqrt(R^2 - (s/2)^2) and R, so s = R corrugates it by 13%
 * - visible rings down a tunnel and a scalloped rim around a hollow. At half a
 * radius the corrugation is 3.2%, or 0.011 units, which is under a voxel at
 * the current grid resolution and therefore cannot be represented, let alone
 * seen. Sculpting tools space their stamps at a quarter to a half of the brush
 * for the same reason.
 */
constexpr float EDIT_ADVANCE = 0.5f * EDIT_RADIUS;

/* The radius over which one carve rounds into the last.
 *
 * Spacing alone cannot fix the rings a string of carves leaves, because they
 * are a crease and not an amplitude: a union of spheres is C0 but not C1, and
 * halving the spacing only halves the crease angle. Rounding the joint is a
 * change of continuity class, which is the only thing that removes it.
 *
 * Sized against the advance rather than the radius, since the advance is what
 * sets how far apart the joints are. Larger eats noticeably more material than
 * was aimed at, because smooth max sits up to k/4 above the hard one.
 */
constexpr float EDIT_BLEND = 0.6f * EDIT_ADVANCE;

// Lattice points per axis for the sampled representation, and the slack added
// around the primitives so the grid holds the surface rather than clipping it.
constexpr uint32_t FIELD_GRID_RESOLUTION = 128;

// Cells to a brick, and the table one object reserves in the bound buffer.
// A grid coarser than the maximum uses the front of its slot and no more.
constexpr uint32_t BRICK_CELLS = DUNYA_BRICK_CELLS;
constexpr uint32_t BRICKS_PER_AXIS =
  (FIELD_GRID_RESOLUTION - 1u + BRICK_CELLS - 1u) / BRICK_CELLS;
constexpr uint32_t MAX_BRICKS_PER_OBJECT =
  BRICKS_PER_AXIS * BRICKS_PER_AXIS * BRICKS_PER_AXIS;
constexpr float FIELD_GRID_MARGIN = 0.5f;

constexpr ObjectId INVALID_OBJECT_ID = UINT32_MAX;
constexpr uint32_t INVALID_PRIMITIVE_OFFSET = UINT32_MAX;

}  // namespace dunya::core
