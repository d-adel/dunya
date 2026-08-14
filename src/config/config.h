#pragma once

#include <cstdint>

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t MAX_PRIMITIVES = 128;
constexpr uint32_t MAX_TEXTURES = 16;
constexpr uint32_t MAX_SAMPLERS = 8;
constexpr uint32_t MAX_MATERIALS = 64;

constexpr uint32_t TEXTURE_WHITE = 0;
constexpr uint32_t TEXTURE_FLAT_NORMAL = 1;
constexpr uint32_t TEXTURE_BLACK = 2;
constexpr uint32_t RESERVED_TEXTURES = 3;

constexpr uint32_t SAMPLER_LINEAR_REPEAT = 0;
