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
  std::vector<dunya::field::Primitive> editList;

  glm::vec3 voxelSize{1.0f};
  glm::uvec3 resolution{0u};

  uint32_t volumeIndex = UINT32_MAX;
  uint32_t unboundedScan = 0;

  bool dirty = true;

  glm::vec4 gridOrigin{0.0f};

  const glm::mat4 inverseModel() const {
    glm::mat4 rotationMatrix = glm::mat4_cast(rotation);

    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

    return glm::inverse(translationMatrix * rotationMatrix);
  }
};

// Spare lanes carry scalars: voxelSize.w is the grid margin, and
// resolutionVolumeIndex.w is the slot the object's volumes occupy.
struct FieldObjectShared {
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
  offsetof(FieldObjectShared, model) == 0,
  "FieldObjectShared must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectShared, inverseModel) == 64,
  "FieldObjectShared must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectShared, voxelSize) == 128,
  "FieldObjectShared must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectShared, resolutionVolumeIndex) == 144,
  "FieldObjectShared must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectShared, config) == 160,
  "FieldObjectShared must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldObjectShared, localOrigin) == 176,
  "FieldObjectShared must match its block in field-shader.frag"
);
static_assert(
  sizeof(FieldObjectShared) == 192,
  "FieldObjectShared must match its block in field-shader.frag"
);

// Fills out with one entry per object. Takes the destination rather than
// returning one, so the per-frame rebuild reuses its storage.
void makeShared(
  uint32_t fieldRepresentation,
  const std::vector<FieldObject>& fieldObjects,
  std::vector<FieldObjectShared>& out
);

dunya::field::Aabb gridBox(const FieldObject& fieldObject);

// Recomputes everything the edit list determines, in one function: refreshing
// some of it and not the rest leaves the bake and shader on different grids.
void refreshDerived(FieldObject& fieldObject);
