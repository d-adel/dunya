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

#endif
