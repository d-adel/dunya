#pragma once

#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/pose/pose.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

namespace dunya::renderer {

struct FieldRecord {
  glm::mat4 model;
  glm::mat4 inverseModel;
  glm::vec4 voxelSize;
  glm::uvec4 resolutionVolumeIndex;
  glm::uvec4 config;
  glm::vec4 localOrigin;
};

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

struct RecordBounds {
  glm::vec4 minimum;
  glm::vec4 maximum;
};

static_assert(
  sizeof(RecordBounds) == 32,
  "RecordBounds must match its block in field-shader.frag"
);

RecordBounds makeRecordBounds(const FieldRecord& record);

FieldRecord makeFieldRecord(
  const dunya::objectmodel::Pose& pose,
  const dunya::objectmodel::SdfGrid& grid,
  const dunya::objectmodel::BakedVolume& volume,
  uint32_t primitiveOffset,
  uint32_t primitiveCount,
  uint32_t fieldRepresentation
);

}
