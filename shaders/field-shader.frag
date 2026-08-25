#version 450
#extension GL_GOOGLE_include_directive : require

const int MAX_MATERIALS = DUNYA_MAX_MATERIALS;
const int MAX_FIELD_OBJECTS = DUNYA_MAX_FIELD_OBJECTS;
const float bias = 0.01;
const float ambient = 0.06;

layout(std140, set = 0, binding = 1) uniform MarchParams {
  float epsilon;
  float maxDistance;
  float omega;
  float gridStepSafety;

  float gradientEpsilon;
  float shadowMaxDistance;
  float shadowSharpness;
  uint maxIterations;
} params;

#include "field-types.glsl"

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

layout(std140, set = 1, binding = 0) uniform MaterialTable {
  Material materials[MAX_MATERIALS];
} materialTable;

layout(std140, set = 0, binding = 0) uniform CameraUniform {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
  mat4 inverseViewProj;
  vec4 position;
} camera;

layout(std430, set = 2, binding = 3) readonly buffer FieldScene {
  Primitive primitives[];
} scene;

struct FieldObjectShared {
  mat4 model;
  mat4 inverseModel;
  vec4 voxelSize;
  uvec4 resolutionVolumeIndex;
  uvec4 config;
  vec4 localOrigin;
};

layout(std140, set = 2, binding = 0) readonly buffer FieldObjectTable {
  FieldObjectShared objects[];
} fieldObjectTable;

const int MAX_SAMPLERS = DUNYA_MAX_SAMPLERS;

layout(set = 1, binding = 2) uniform sampler samplers[MAX_SAMPLERS];

layout(set = 2, binding = 1)
  uniform texture3D distanceVolume[MAX_FIELD_OBJECTS];

layout(set = 2, binding = 2)
  uniform utexture3D materialVolume[MAX_FIELD_OBJECTS];

layout(location = 0) in vec4 clipPosition;
layout(location = 1) flat in uint objectIndex;

layout(location = 0) out vec4 outColor;

uint fieldPrimitiveOffset = 0u;
uint fieldPrimitiveCount = 0u;

// config.x = primitive count
// config.y = field representation
// config.z = live flag
// config.w = primitive offset
#define FIELD_PRIMITIVE_AT(i) scene.primitives[fieldPrimitiveOffset + (i)]

#define FIELD_PRIMITIVE_COUNT fieldPrimitiveCount

#include "field-common.glsl"

float outsideGrid(FieldObjectShared fieldObject, vec3 p) {
  vec3 maxCorner = fieldObject.localOrigin.xyz
                   + fieldObject.voxelSize.xyz
                       * vec3(fieldObject.resolutionVolumeIndex.xyz - 1u);

  return length(p - clamp(p, fieldObject.localOrigin.xyz, maxCorner));
}

vec2 gridSample(FieldObjectShared fieldObject, vec3 p) {
  uint volumeIndex = fieldObject.resolutionVolumeIndex.w;

  vec3 lattice = (p - fieldObject.localOrigin.xyz) / fieldObject.voxelSize.xyz;

  vec3 uvw = (lattice + 0.5) / vec3(fieldObject.resolutionVolumeIndex.xyz);

  float distance = texture(sampler3D(distanceVolume[volumeIndex],
                                     samplers[DUNYA_SAMPLER_LINEAR_CLAMP]),
                           uvw)
                     .r;

  uint material = texture(usampler3D(materialVolume[volumeIndex],
                                     samplers[DUNYA_SAMPLER_NEAREST_CLAMP]),
                          uvw)
                    .r;

  return vec2(distance * params.gridStepSafety, float(material));
}

vec2 fieldDistance(FieldObjectShared fieldObject, vec3 p) {
  if (fieldObject.config.y == 0u) {
    fieldPrimitiveOffset = fieldObject.config.w;
    fieldPrimitiveCount = fieldObject.config.x;
    return sceneDistance(p);
  }

  float outside = outsideGrid(fieldObject, p);

  if (outside > 0.0) {
    return vec2(outside + fieldObject.voxelSize.w, 0.0);
  }

  return gridSample(fieldObject, p);
}

float gradientOffset(FieldObjectShared fieldObject) {
  if (fieldObject.config.y == 0u) {
    return params.gradientEpsilon;
  }

  vec3 voxel = fieldObject.voxelSize.xyz;

  return max(params.gradientEpsilon, max(voxel.x, max(voxel.y, voxel.z)));
}

float surfaceBias(FieldObjectShared fieldObject) {
  if (fieldObject.config.y == 0u) {
    return bias;
  }

  vec3 voxel = fieldObject.voxelSize.xyz;

  return max(bias, 2.0 * max(voxel.x, max(voxel.y, voxel.z)));
}

vec3 estimateNormal(FieldObjectShared fieldObject, vec3 p) {
  float h = gradientOffset(fieldObject);

  vec3 offsetX = vec3(h, 0.0, 0.0);
  vec3 offsetY = vec3(0.0, h, 0.0);
  vec3 offsetZ = vec3(0.0, 0.0, h);

  return normalize(vec3(fieldDistance(fieldObject, p + offsetX).x
                          - fieldDistance(fieldObject, p - offsetX).x,

                        fieldDistance(fieldObject, p + offsetY).x
                          - fieldDistance(fieldObject, p - offsetY).x,

                        fieldDistance(fieldObject, p + offsetZ).x
                          - fieldDistance(fieldObject, p - offsetZ).x));
}

bool march(FieldObjectShared fieldObject,
           vec3 origin,
           vec3 direction,
           out vec3 hitPosition,
           out float materialId) {
  float distanceTravelled = 0.0;

  float omega = params.omega;
  float previousRadius = 0.0;
  float stepLength = 0.0;
  float functionSign = 1.0;

  for (int i = 0; i < int(params.maxIterations); ++i) {
    vec3 p = origin + direction * distanceTravelled;

    vec2 field = fieldDistance(fieldObject, p);

    if (i == 0) {
      functionSign = field.x < 0.0 ? -1.0 : 1.0;
    }

    float signedRadius = functionSign * field.x;

    float radius = abs(signedRadius);

    bool relaxationFailed =
      omega > 1.0 && (radius + previousRadius) < stepLength;

    if (relaxationFailed) {
      stepLength -= omega * stepLength;
      omega = 1.0;
    } else {
      stepLength = signedRadius * omega;
    }

    previousRadius = radius;

    if (!relaxationFailed && radius <= params.epsilon) {
      hitPosition = p;
      materialId = field.y;
      return true;
    }

    distanceTravelled += stepLength;

    if (distanceTravelled > params.maxDistance) {
      break;
    }
  }

  return false;
}

float lightReachingLocal(FieldObjectShared fieldObject,
                         vec3 origin,
                         vec3 direction) {
  float result = 1.0;
  float distanceTravelled = 0.0;

  for (int i = 0; i < int(params.maxIterations); ++i) {
    vec3 p = origin + direction * distanceTravelled;

    bool trueDistance = true;
    float distanceToSurface;

    if (fieldObject.config.y == 0u) {
      // Analytic field: fieldDistance is meaningful everywhere.
      distanceToSurface = fieldDistance(fieldObject, p).x;
    } else {
      float outside = outsideGrid(fieldObject, p);

      if (outside > 0.0) {
        // Outside a sampled object's grid this is only a conservative
        // marching bound to the grid, not distance to actual geometry.
        distanceToSurface = outside + fieldObject.voxelSize.w;

        trueDistance = false;
      } else {
        // Inside the grid we have an actual sampled field distance.
        distanceToSurface = fieldDistance(fieldObject, p).x;
      }
    }

    // Only real geometry distances can report an occluder or influence
    // the soft-shadow cone. A grid-box bound is only allowed to advance
    // the ray.
    if (trueDistance) {
      if (distanceToSurface <= params.epsilon) {
        return 0.0;
      }

      if (distanceTravelled > 0.0) {
        result =
          min(result,
              params.shadowSharpness * distanceToSurface / distanceTravelled);
      }
    }

    distanceTravelled += distanceToSurface;

    if (distanceTravelled > params.shadowMaxDistance || result < 0.01) {
      break;
    }
  }

  return clamp(result, 0.0, 1.0);
}

float lightReaching(vec3 worldOrigin, vec3 worldDirection) {
  float result = 1.0;

  for (uint i = 0u; i < uint(MAX_FIELD_OBJECTS); ++i) {
    FieldObjectShared object = fieldObjectTable.objects[i];

    // config.z = liveness.
    //
    // Dead slots are zero-filled. In particular their volume index is zero,
    // which is a real volume, so they absolutely must not be marched.
    if (object.config.z == 0u) {
      continue;
    }

    // Same world -> local crossing as the primary ray:
    // point w = 1
    // direction w = 0
    //
    // Do not renormalise the direction.
    vec3 localOrigin = (object.inverseModel * vec4(worldOrigin, 1.0)).xyz;

    vec3 localDirection = (object.inverseModel * vec4(worldDirection, 0.0)).xyz;

    float objectLight = lightReachingLocal(object, localOrigin, localDirection);

    result = min(result, objectLight);

    // A hard occluder cannot get any darker.
    if (result <= 0.0) {
      break;
    }
  }

  return result;
}

vec3 albedo(float materialId) {
  return materialTable.materials[uint(materialId + 0.5)].baseColor.rgb;
}

void main() {
  FieldObjectShared fieldObject = fieldObjectTable.objects[objectIndex];

  vec2 ndc = clipPosition.xy / clipPosition.w;

  vec4 worldPosition = camera.inverseViewProj * vec4(ndc, 1.0, 1.0);

  worldPosition /= worldPosition.w;

  vec3 worldDirection = normalize(worldPosition.xyz - camera.position.xyz);

  vec3 localDirection =
    (fieldObject.inverseModel * vec4(worldDirection, 0.0)).xyz;

  vec3 localCameraPos =
    (fieldObject.inverseModel * vec4(camera.position.xyz, 1.0)).xyz;

  vec3 localHitPosition;
  float materialId;

  if (!march(fieldObject,
             localCameraPos,
             localDirection,
             localHitPosition,
             materialId)) {
    discard;
  }

  vec3 surfaceAlbedo = albedo(materialId);

  vec4 worldHitPos = fieldObject.model * vec4(localHitPosition, 1.0);

  vec4 clip = camera.viewProj * worldHitPos;

  float depth = clip.z / clip.w;

  if (depth <= 0.0 || isinf(depth) || isnan(depth)) {
    discard;
  }

  gl_FragDepth = depth;

  vec3 localNormal = estimateNormal(fieldObject, localHitPosition);

  // Face the normal toward the viewer for interior surfaces.
  if (dot(localNormal, localDirection) > 0.0) {
    localNormal = -localNormal;
  }

  // Rigid transform, so the model's rotational part is sufficient.
  vec3 worldNormal =
    normalize((fieldObject.model * vec4(localNormal, 0.0)).xyz);

  vec3 worldLightDir = normalize(vec3(0.4, 1.0, 0.6));

  float diffuse = max(0.0, dot(worldNormal, worldLightDir));

  float departureBias = surfaceBias(fieldObject);

  vec3 shadowOrigin = worldHitPos.xyz + worldNormal * departureBias;

  float light =
    diffuse > 0.0 ? lightReaching(shadowOrigin, worldLightDir) : 1.0;

  vec3 color = surfaceAlbedo * (ambient + diffuse * light);

  outColor = vec4(color, 1.0);
}
