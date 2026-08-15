#pragma once

#include <cstdint>

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// The shaders declare these too, so the numbers come from CMake, which is the
// only place that reaches both compilers.
constexpr uint32_t MAX_PRIMITIVES = DUNYA_MAX_PRIMITIVES;
constexpr uint32_t MAX_TEXTURES = DUNYA_MAX_TEXTURES;
constexpr uint32_t MAX_SAMPLERS = DUNYA_MAX_SAMPLERS;
constexpr uint32_t MAX_MATERIALS = DUNYA_MAX_MATERIALS;

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

constexpr float EDIT_RADIUS = 0.35f;

// Lattice points per axis for the sampled representation, and the slack added
// around the primitives so the grid holds the surface rather than clipping it.
constexpr uint32_t FIELD_GRID_RESOLUTION = 128;
constexpr float FIELD_GRID_MARGIN = 0.5f;
