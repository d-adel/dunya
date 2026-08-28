#ifndef DUNYA_FIELD_TYPES_GLSL
#define DUNYA_FIELD_TYPES_GLSL

// Declared once and included rather than restated. Must match Primitive in
// src/dunya/field/field.h, whose static_assert pins it at 112 bytes.

struct Primitive {
  mat4 inverseModel;
  vec4 shape;
  uvec4 shapeConfig;
  vec4 bounds;
};

// The per-frame record, as both field shaders read it. Must match FieldRecord
// in src/dunya/renderer/fieldrecord/fieldrecord.h, whose static_asserts pin it.
struct FieldRecordShared {
  mat4 model;
  mat4 inverseModel;
  vec4 voxelSize;
  uvec4 resolutionVolumeIndex;
  uvec4 config;
  vec4 localOrigin;
};

#endif
