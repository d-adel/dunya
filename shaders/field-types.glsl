#ifndef DUNYA_FIELD_TYPES_GLSL
#define DUNYA_FIELD_TYPES_GLSL

/* Layout-critical, so it is declared once and included rather than restated.
 * Must match Primitive in src/field/field.h, whose static_assert pins its size
 * at 112 bytes.
 *
 * Separate from field-common.glsl because a buffer block naming this type has
 * to be declared before the functions that read the block, so the two halves
 * are included at different points.
 */

struct Primitive {
  mat4 inverseModel;
  vec4 shape;
  uvec4 shapeConfig;
  vec4 bounds;
};

/* The per-frame record of a field entity, as both field shaders read it.
 * Must match FieldRecord in src/dunya/renderer/fieldrecord/fieldrecord.h,
 * whose static_asserts pin every member offset and the 192-byte total. It used
 * to be restated in field-shader.frag and field-shader.vert; two copies of a
 * byte layout is idiom 13 waiting to happen.
 */
struct FieldRecordShared {
  mat4 model;
  mat4 inverseModel;
  vec4 voxelSize;
  uvec4 resolutionVolumeIndex;
  uvec4 config;
  vec4 localOrigin;
};

#endif
