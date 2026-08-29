#pragma once

#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/pose/pose.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

namespace dunya::renderer {

// A frame's record of a field entity, packed the way field-shader.frag reads
// it. Derives model, inverseModel and the bound offset on the way in.
//
// voxelSize.w: grid margin
// resolutionVolumeIndex.w: volume index
// config.x = primitive count
// config.y = field representation: 0 = analytical, 1 = sampled
// config.z = live; written but no longer read, the shadow loop is bounded by
//            SceneCounts.fieldRecords
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

// The record's grid box in world space, kept in its own array so the shadow
// loop can reject a record without reading the 192 bytes that describe it.
// Six hundred records is 118 KB of FieldRecord and 19 KB of this, which is
// the difference between thrashing a cache and fitting in one.
//
// Two vec4s rather than two vec3s because std430 pads a vec3 to 16 bytes
// anyway, so the tighter spelling would cost the same and read worse.
struct RecordBounds {
  glm::vec4 minimum;
  glm::vec4 maximum;
};

static_assert(
  sizeof(RecordBounds) == 32,
  "RecordBounds must match its block in field-shader.frag"
);

// Derived rather than passed: the record already carries the local box and the
// transform, and two places computing the same world box is how they drift.
RecordBounds makeRecordBounds(const FieldRecord& record);

FieldRecord makeFieldRecord(
  const dunya::objectmodel::Pose& pose,
  const dunya::objectmodel::SdfGrid& grid,
  const dunya::objectmodel::BakedVolume& volume,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
);

}  // namespace dunya::renderer
