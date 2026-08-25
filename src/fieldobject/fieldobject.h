#pragma once

#include "field/field.h"
#include "field/analytic.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstddef>
#include <cstdint>

struct FieldObject {
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};

  uint32_t volumeIndex = UINT32_MAX;

  bool dirty = true;

  glm::vec4 gridOrigin{0.0f};

  const glm::mat4 inverseModel() const {
    glm::mat4 rotationMatrix = glm::mat4_cast(rotation);

    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

    return glm::inverse(translationMatrix * rotationMatrix);
  }
};

// voxelSize.w: grid margin
// resolutionVolumeIndex.w: volume index
// config.x = primitive count
// config.y = field representation: 0 = analytical, 1 = sampled
// config.z = live
// config.w = primitive offset
struct FieldObjectGPU {
  glm::mat4 model;                   // 64 bytes (offset 0)
  glm::mat4 inverseModel;            // 64 bytes (offset 64)
  glm::vec4 voxelSize;               // 16 bytes (offset 128)
  glm::uvec4 resolutionVolumeIndex;  // 16 bytes (offset 144)
  glm::uvec4 config;                 // 16 bytes (offset 160)
  glm::vec4 localOrigin;             // 16 bytes (offset 176)
                                     // Total: 192 bytes
};

// Pinned because the shader reads these bytes by position.
static_assert(
  offsetof(FieldObjectGPU, model) == 0,
  "FieldObjectGPU must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectGPU, inverseModel) == 64,
  "FieldObjectGPU must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectGPU, voxelSize) == 128,
  "FieldObjectGPU must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectGPU, resolutionVolumeIndex) == 144,
  "FieldObjectGPU must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectGPU, config) == 160,
  "FieldObjectGPU must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectGPU, localOrigin) == 176,
  "FieldObjectGPU must match its block in field-shader.frag"
);
static_assert(
  sizeof(FieldObjectGPU) == 192,
  "FieldObjectGPU must match its block in field-shader.frag"
);

FieldObjectGPU fromFieldObject(
  const FieldObject& fieldObject,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
);

dunya::field::Aabb gridBox(std::span<const dunya::field::Primitive> primitives);

void refreshDerived(
  FieldObject& fieldObject,
  std::span<const dunya::field::Primitive> primitives
);
