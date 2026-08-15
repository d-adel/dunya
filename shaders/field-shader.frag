#version 450

const int MAX_MATERIALS = DUNYA_MAX_MATERIALS;
const float eps = DUNYA_MARCH_EPSILON;
const int maxIter = DUNYA_MARCH_MAX_ITERATIONS;
const float maxTravel = DUNYA_MARCH_MAX_DISTANCE;
const float shadowMaxTravel = DUNYA_SHADOW_MAX_DISTANCE;
const float normalSampleOffset = DUNYA_GRADIENT_EPSILON;
const float omegaStart = DUNYA_MARCH_OMEGA;
const float shadowSharpness = DUNYA_SHADOW_SHARPNESS;
const float bias = 0.01;
const float ambient = 0.06;

struct Primitive {
  mat4 inverseModel;
  vec4 shape;
  uvec4 shapeConfig;
  vec4 bounds;
};

struct Material {
  vec4 baseColor;
  vec4 emissive;

  float metallic;
  float roughness;
  float normalScale;
  float occlusionStrength;

  float alphaCutoff;
  uint flags;
  uint baseColorTexture;
  uint baseColorSampler;

  uint metallicRoughnessTexture;
  uint metallicRoughnessSampler;
  uint normalTexture;
  uint normalSampler;

  uint occlusionTexture;
  uint occlusionSampler;
  uint emissiveTexture;
  uint emissiveSampler;
};

layout(std140, set = 1, binding = 0) uniform
MaterialTable { Material materials[MAX_MATERIALS]; } materialTable;

layout(std140, set = 0, binding = 0) uniform CameraUniform {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
  mat4 inverseViewProj;
  vec4 position;
} camera;

// A storage buffer rather than a uniform one: the primitive array is scene
// data that grows at runtime, and the guaranteed uniform range is 16 KB.
// std430 and std140 agree here because every member is 16-byte aligned and
// Primitive is exactly 96 bytes, which its static_assert pins.
layout(std430, set = 2, binding = 0) readonly buffer
FieldScene { Primitive primitives[]; } scene;

layout(std140, set = 2, binding = 1) uniform FieldFrame {
  uvec4 primitiveCount;
} frame;

layout(location = 0) in vec4 ndc;
layout(location = 0) out vec4 outColor;

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

// Mirrors skippable() in analytic.cc. A radius of zero means no bound is
// known, so the primitive is always evaluated.
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
      return bound >= -acc;
    default:
      return false;
  }
}

vec2 sceneDistance(vec3 p) {
  vec2 acc = vec2(1e9, 0);
  for (uint i = 0u; i < frame.primitiveCount.x; ++i) {
    if (skippable(p, scene.primitives[i], acc.x)) {
      continue;
    }

    vec2 cur = vec2(primitiveDistance(p, scene.primitives[i]),
    float(scene.primitives[i].shapeConfig.y));
    switch(scene.primitives[i].shapeConfig.z) {
      case 1u:
        {
          float k = scene.primitives[i].shape.w;
          acc = vec2(smin(acc.x, cur.x, k), acc.x < cur.x ? acc.y : cur.y);
        }
        break;
      case 2u:
        acc = acc.x > cur.x ? acc : cur;
        break;
      case 3u:
        acc = vec2(max(acc.x, -cur.x), acc.y);
        break;
      default:
        acc = minMat(acc, cur);
        break;
    }
  }

  return acc;
}

vec3 estimateNormal(vec3 p)
{
    vec3 offsetX = vec3(normalSampleOffset, 0.0, 0.0);
    vec3 offsetY = vec3(0.0, normalSampleOffset, 0.0);
    vec3 offsetZ = vec3(0.0, 0.0, normalSampleOffset);

    return normalize(vec3(
        sceneDistance(p + offsetX).x - sceneDistance(p - offsetX).x,
        sceneDistance(p + offsetY).x - sceneDistance(p - offsetY).x,
        sceneDistance(p + offsetZ).x - sceneDistance(p - offsetZ).x
    ));
}

// Steps by the unsigned distance so a ray that starts inside solid geometry
// marches out to the boundary instead of reporting a hit where it began. That
// is what lets the camera fly inside a shape and see its interior.
bool march(vec3 origin, vec3 direction, out vec3 hitPosition, out float materialId)
{
    float distanceTravelled = 0;

    float omega = omegaStart;
    float previousRadius = 0.0;
    float stepLength = 0.0;
    float functionSign = 1.0;

    for (int i = 0; i < maxIter; ++i)
    {
        vec3 p =
            origin +
            direction * distanceTravelled;

        vec2 field = sceneDistance(p);

        if (i == 0)
        {
            functionSign = field.x < 0.0 ? -1.0 : 1.0;
        }

        float signedRadius = functionSign * field.x;
        float radius = abs(signedRadius);

        // The over-stepped sphere no longer reaches the previous one, so the
        // step may have skipped a surface. Undo part of it and stop relaxing.
        bool relaxationFailed =
            omega > 1.0 && (radius + previousRadius) < stepLength;

        if (relaxationFailed)
        {
            stepLength -= omega * stepLength;
            omega = 1.0;
        }
        else
        {
            stepLength = signedRadius * omega;
        }

        previousRadius = radius;

        if (!relaxationFailed && radius <= eps)
        {
            hitPosition = p;
            materialId = field.y;
            return true;
        }

        distanceTravelled += stepLength;

        if (distanceTravelled > maxTravel)
        {
            break;
        }
    }

    return false;
}

// Returns how much light reaches the point, not whether anything blocked it.
// The closest the ray passes to a surface, relative to how far it has gone, is
// an approximate cone intersection and costs nothing beyond the march we were
// doing anyway - which is where soft shadows come from for free. Owns nothing
// and answers one question (idiom 23), and stops early once it is fully dark.
float lightReaching(vec3 origin, vec3 direction)
{
    float result = 1.0;
    float distanceTravelled = bias;

    for (int i = 0; i < maxIter; ++i)
    {
        float distanceToSurface =
            sceneDistance(origin + direction * distanceTravelled).x;

        if (distanceToSurface <= eps)
        {
            return 0.0;
        }

        result = min(result, shadowSharpness * distanceToSurface / distanceTravelled);

        distanceTravelled += distanceToSurface;

        if (distanceTravelled > shadowMaxTravel || result < 0.01)
        {
            break;
        }
    }

    return clamp(result, 0.0, 1.0);
}

vec3 albedo(float materialId) {
  return materialTable.materials[uint(materialId + 0.5)].baseColor.rgb;
}

void main()
{

    vec4 clipPosition = vec4(ndc.xy, 1.0, 1.0);

    vec4 worldPosition =
        camera.inverseViewProj * clipPosition;

    worldPosition /= worldPosition.w;

    vec3 direction = normalize(
        worldPosition.xyz - camera.position.xyz
    );

    vec3 hitPosition;
    float materialId;
    if (march(camera.position.xyz, direction, hitPosition, materialId))
    {
      vec3 surfaceAlbedo = albedo(materialId);

      vec4 clip = camera.viewProj * vec4(hitPosition, 1.0);
      float depth = clip.z / clip.w;
      if (depth <= 0 || isinf(depth) || isnan(depth)) {
        discard;
      }
      gl_FragDepth = depth;

      vec3 lightDir = normalize(vec3(0.4, 1.0, 0.6));
      vec3 normal = estimateNormal(hitPosition);

      // The gradient points out of solid geometry, which is away from the eye
      // when the surface is being viewed from inside. Face it back at the
      // viewer so an interior wall is shaded rather than left black.
      if (dot(normal, direction) > 0.0) {
        normal = -normal;
      }

      float diffuse = max(0.0, dot(normal, lightDir));

      float light = diffuse > 0.0
        ? lightReaching(hitPosition + normal * bias, lightDir)
        : 1.0;

      vec3 color = vec3(surfaceAlbedo * (ambient + diffuse * light));
      outColor = vec4(color, 1.0);
    }
    else
    {
        discard;
    }
}
