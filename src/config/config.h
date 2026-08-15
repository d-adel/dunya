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

constexpr uint32_t SAMPLER_LINEAR_REPEAT = 0;

// Operation ids as the field shader's shapeConfig.z reads them.
constexpr uint32_t FIELD_OP_UNION = 0;
constexpr uint32_t FIELD_OP_SMOOTH_UNION = 1;
constexpr uint32_t FIELD_OP_INTERSECTION = 2;
constexpr uint32_t FIELD_OP_SUBTRACTION = 3;

constexpr float EDIT_RADIUS = 0.35f;
