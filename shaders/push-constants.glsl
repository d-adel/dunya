#ifndef DUNYA_PUSH_CONSTANTS_GLSL
#define DUNYA_PUSH_CONSTANTS_GLSL

layout(push_constant) uniform PushConstants {
  mat4 model;
  uint materialIndex;
  uint recordIndex;
} push;

#endif
