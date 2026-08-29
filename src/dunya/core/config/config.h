#pragma once

#include <cstdint>

namespace dunya::core {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// The shaders declare these too, so the numbers come from CMake, which is the
// only place that reaches both compilers.
constexpr uint32_t MAX_TEXTURES = DUNYA_MAX_TEXTURES;
constexpr uint32_t MAX_SAMPLERS = DUNYA_MAX_SAMPLERS;
constexpr uint32_t MAX_MATERIALS = DUNYA_MAX_MATERIALS;
constexpr uint32_t MAX_FIELD_RECORDS = DUNYA_MAX_FIELD_RECORDS;
constexpr uint32_t MAX_FIELD_VOLUMES = DUNYA_MAX_FIELD_VOLUMES;
constexpr uint32_t MAX_FIELD_PRIMITIVES = DUNYA_MAX_FIELD_PRIMITIVES;
constexpr size_t MAX_PRIMITIVE_POOL = DUNYA_MAX_PRIMITIVE_POOL;

static_assert(
  MAX_MATERIALS <= 256,
  "A material id is stored as a byte in the lattice and read as R8_UINT"
);

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

// Its own id rather than a blend radius on the hard one. It also keeps the
// degenerate case out: smin at k = 0 is min by way of 0/0 (idiom 30).
constexpr uint32_t FIELD_OP_SMOOTH_SUBTRACTION = 4;

// Which family an operation belongs to, asked in one place. Callers care about
// the direction material moves, not about an id, and every downstream `== 3`
// is a site that a fifth operation would silently walk past (idiom 24).
constexpr bool fieldOpRemovesMaterial(uint32_t operation) {
  return operation == FIELD_OP_SUBTRACTION
         || operation == FIELD_OP_SMOOTH_SUBTRACTION;
}

constexpr float EDIT_RADIUS = 0.35f;

// How far one click moves the surface, which is not how wide a bite is. A full
// radius is the worst spacing: equal spheres corrugate the wall by 13%.
constexpr float EDIT_ADVANCE = 0.5f * EDIT_RADIUS;

// The radius over which one carve rounds into the last. Spacing cannot fix the
// rings - they are a crease, not an amplitude.
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

// Each object's range opens with one bound covering its whole grid, then the
// per-brick ones. The global sits first so a shader can read it from the
// range's own address without knowing how many bricks follow.
constexpr uint32_t BRICK_TABLE_STRIDE = 1u + MAX_BRICKS_PER_OBJECT;

// An object's first entry in the bound table travels to the shader as a float,
// which represents integers exactly only up to 2^24. Raising either capacity
// far enough would silently round the address of a later object's slot.
static_assert(
  static_cast<uint64_t>(MAX_FIELD_VOLUMES) * BRICK_TABLE_STRIDE < (1ull << 24),
  "The bound table's largest index must stay exact in a float"
);
constexpr float FIELD_GRID_MARGIN = 0.5f;

constexpr uint32_t INVALID_PRIMITIVE_OFFSET = UINT32_MAX;

}  // namespace dunya::core
