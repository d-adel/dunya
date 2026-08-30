#ifndef DUNYA_FIELD_COMMON_GLSL
#define DUNYA_FIELD_COMMON_GLSL

#include "sdf-types.glsl"

float smin(float a, float b, float k)
{
    float h = clamp(
        0.5 + 0.5 * (b - a) / k,
        0.0,
        1.0
    );

    return mix(b, a, h) - k * h * (1.0 - h);
}

float sdBox(vec3 p, vec3 center, vec3 halfSize)
{
    vec3 q = abs(p - center) - halfSize;

    float outsideDistance = length(max(q, vec3(0.0)));
    float insideDistance = min(max(q.x, max(q.y, q.z)), 0.0);

    return outsideDistance + insideDistance;
}

vec2 minMat(vec2 a, vec2 b) {
  return a.x < b.x ? a : b;
}

float primitiveDistance(vec3 p, Primitive prim) {
  vec3 local = ((prim.inverseModel) * vec4(p, 1.0)).xyz;
  switch(prim.shapeConfig.x) {
    case 0u:
      return length(local) - prim.shape.x;
    case 1u:
      return sdBox(local, vec3(0), prim.shape.xyz);
    case 2u:
      return local.y;
    default:
      return 1e9;
  }
}

bool skippable(vec3 p, Primitive prim, float acc) {
  if (prim.bounds.w <= 0.0) {
    return false;
  }

  float bound = length(p - prim.bounds.xyz) - prim.bounds.w;

  switch (prim.shapeConfig.z) {
    case 0u:
    case 1u:
      return bound > acc;
    case 3u:
    case 4u:
      return bound >= -acc;
    default:
      return false;
  }
}

vec2 foldDistance(vec3 p, uint count, bool unboundedOnly) {
  vec2 acc = vec2(1e9, 0);
  for (uint i = 0u; i < count; ++i) {
    Primitive prim = FIELD_PRIMITIVE_AT(i);

    if (unboundedOnly && prim.bounds.w > 0.0) {
      continue;
    }

    if (skippable(p, prim, acc.x)) {
      continue;
    }

    vec2 cur = vec2(primitiveDistance(p, prim), float(prim.shapeConfig.y));

    switch(prim.shapeConfig.z) {
      case 1u:
        {
          float k = prim.shape.w;
          acc = vec2(smin(acc.x, cur.x, k), acc.x < cur.x ? acc.y : cur.y);
        }
        break;
      case 2u:
        acc = acc.x > cur.x ? acc : cur;
        break;
      case 3u:
        acc = vec2(max(acc.x, -cur.x), acc.y);
        break;
      case 4u:
        acc = vec2(-smin(-acc.x, cur.x, prim.shape.w), acc.y);
        break;
      default:
        acc = minMat(acc, cur);
        break;
    }
  }

  return acc;
}

vec2 sceneDistance(vec3 p) {
  return foldDistance(p, FIELD_PRIMITIVE_COUNT, false);
}

#endif
