#pragma once

#include <dunya/objectmodel/fieldobject/fieldobject.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

namespace dunya::renderer {

// A frame's record of a field entity, packed the way field-shader.frag reads
// it. Not a mirror of any one CPU type - it carries the pose, the grid, the
// primitive window and the bound offset together, and derives model,
// inverseModel and that offset on the way in.
//
// voxelSize.w: grid margin
// resolutionVolumeIndex.w: volume index
// config.x = primitive count
// config.y = field representation: 0 = analytical, 1 = sampled
// config.z = live
// config.w = primitive offset
struct FieldRecord {
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
  offsetof(FieldRecord, model) == 0,
  "FieldRecord must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldRecord, inverseModel) == 64,
  "FieldRecord must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldRecord, voxelSize) == 128,
  "FieldRecord must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldRecord, resolutionVolumeIndex) == 144,
  "FieldRecord must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldRecord, config) == 160,
  "FieldRecord must match its block in field-shader.frag"
);
static_assert(
  offsetof(FieldRecord, localOrigin) == 176,
  "FieldRecord must match its block in field-shader.frag"
);
static_assert(
  sizeof(FieldRecord) == 192,
  "FieldRecord must match its block in field-shader.frag"
);

FieldRecord makeFieldRecord(
  const dunya::objectmodel::FieldObject& fieldObject,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
);

}  // namespace dunya::renderer
