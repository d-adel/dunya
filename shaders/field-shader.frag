#version 450
#extension GL_GOOGLE_include_directive : require

const int MAX_MATERIALS = DUNYA_MAX_MATERIALS;
const int MAX_FIELD_OBJECTS = DUNYA_MAX_FIELD_OBJECTS;
const float bias = 0.01;
const float ambient = 0.06;

// Tunable at runtime rather than compiled in: turning one of these used to mean
// rebuilding two toolchains, which is not tuning. All scalars, so std140 packs
// them consecutively - MarchParams in frameglobals.h holds the matching struct
// and a static_assert on its size.
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

// A storage buffer rather than a uniform one: the primitive array is scene
// data that grows at runtime, and the guaranteed uniform range is 16 KB.
// std430 and std140 agree here because every member is 16-byte aligned and
// Primitive is exactly 96 bytes, which its static_assert pins.
layout(std430, set = 2, binding = 3) readonly buffer FieldScene {
  Primitive primitives[];
} scene;

// layout(std140, set = 2, binding = 1) uniform FieldFrame {
//   uvec4 config;
//   vec4 gridOrigin;
//   vec4 gridVoxelSize;
//   uvec4 gridResolution;
// } frame;

layout(std140, set = 2, binding = 0) readonly buffer FieldObjectShared {
  mat4 model;
  mat4 inverseModel;
  vec4 voxelSize;
  uvec4 resolutionVolumeIndex;
  uvec4 config;
  vec4 localOrigin;
} fieldObject;

uint volumeIndex = fieldObject.resolutionVolumeIndex.w;

const int MAX_SAMPLERS = DUNYA_MAX_SAMPLERS;
layout(set = 1, binding = 2) uniform sampler samplers[MAX_SAMPLERS];

layout(set = 2, binding = 1)
  uniform texture3D distanceVolume[MAX_FIELD_OBJECTS];
layout(set = 2, binding = 2)
  uniform utexture3D materialVolume[MAX_FIELD_OBJECTS];

layout(location = 0) in vec4 ndc;
layout(location = 0) out vec4 outColor;

#define FIELD_PRIMITIVE_AT(i) scene.primitives[i]
#define FIELD_PRIMITIVE_COUNT fieldObject.config.x

#include "field-common.glsl"

// How far outside the grid's box a point lies, and zero when it is inside.
float outsideGrid(vec3 p) {
  vec3 maxCorner = fieldObject.localOrigin.xyz
                   + fieldObject.voxelSize.xyz
                       * vec3(fieldObject.resolutionVolumeIndex.xyz - 1u);

  return length(p - clamp(p, fieldObject.localOrigin.xyz, maxCorner));
}

// Values sit at lattice points, so lattice index i is texel i, whose centre in
// normalised coordinates is (i + 0.5) / N. Only valid inside the box.
vec2 gridSample(vec3 p) {
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

  // Trilinear interpolation overestimates - measured in sampled_tests.cc - so
  // the step is shortened rather than trusted. At the surface the distance is
  // zero, so scaling moves no surface; it only makes the march creep.
  return vec2(distance * params.gridStepSafety, float(material));
}

/* The sampled representation, which is the grid where there is one and the
 * analytic field where there is not.
 *
 * The split is by region, not by primitive. The bake runs the whole fold, so
 * inside the box the grid is already the entire scene - carved ground and all -
 * and mixing the analytic ground back in would put back exactly the material a
 * subtraction had just removed. Outside the box only unbounded primitives can
 * reach, because the bounded ones are what the box was sized around.
 */
vec2 fieldDistance(vec3 p) {
  if (fieldObject.config.y == 0u) {
    return sceneDistance(p);
  }

  float outside = outsideGrid(p);

  if (outside > 0.0) {
    return minMat(vec2(outside + fieldObject.voxelSize.w, 0.0),
                  foldDistance(p, fieldObject.config.z, true));
  }

  return gridSample(p);
}

// Trilinear interpolation is only C0: the value is continuous across a cell
// boundary but the slope is not, so a difference taken inside one cell returns
// that cell's own slope and neighbouring cells disagree. That disagreement is
// the faceting - it is shading, not geometry, which is why the silhouette stays
// smooth while the surface looks chiselled. Spanning a whole cell averages the
// two slopes instead of picking one.
float gradientOffset() {
  if (fieldObject.config.y == 0u) {
    return params.gradientEpsilon;
  }

  vec3 voxel = fieldObject.voxelSize.xyz;

  return max(params.gradientEpsilon, max(voxel.x, max(voxel.y, voxel.z)));
}

/* How far off a surface anything that starts on it has to begin.
 *
 * The grid only knows where the surface is to within its own interpolation
 * error, so a bias thinner than a voxel starts a shadow ray inside the surface
 * it just left, and the march reports that surface as its own occluder. Since
 * lightReaching returns a hard zero, neighbouring pixels disagreeing about
 * whether they started inside come out as black teeth against lit ones rather
 * than as a soft terminator.
 */
float surfaceBias() {
  if (fieldObject.config.y == 0u) {
    return bias;
  }

  vec3 voxel = fieldObject.voxelSize.xyz;

  return max(bias, 2.0 * max(voxel.x, max(voxel.y, voxel.z)));
}

vec3 estimateNormal(vec3 p) {
  float h = gradientOffset();

  vec3 offsetX = vec3(h, 0.0, 0.0);
  vec3 offsetY = vec3(0.0, h, 0.0);
  vec3 offsetZ = vec3(0.0, 0.0, h);

  return normalize(
    vec3(fieldDistance(p + offsetX).x - fieldDistance(p - offsetX).x,
         fieldDistance(p + offsetY).x - fieldDistance(p - offsetY).x,
         fieldDistance(p + offsetZ).x - fieldDistance(p - offsetZ).x));
}

// Steps by the unsigned distance so a ray that starts inside solid geometry
// marches out to the boundary instead of reporting a hit where it began. That
// is what lets the camera fly inside a shape and see its interior.
bool march(vec3 origin,
           vec3 direction,
           out vec3 hitPosition,
           out float materialId) {
  float distanceTravelled = 0;

  float omega = params.omega;
  float previousRadius = 0.0;
  float stepLength = 0.0;
  float functionSign = 1.0;

  for (int i = 0; i < int(params.maxIterations); ++i) {
    vec3 p = origin + direction * distanceTravelled;

    vec2 field = fieldDistance(p);

    if (i == 0) {
      functionSign = field.x < 0.0 ? -1.0 : 1.0;
    }

    float signedRadius = functionSign * field.x;
    float radius = abs(signedRadius);

    // The over-stepped sphere no longer reaches the previous one, so the
    // step may have skipped a surface. Undo part of it and stop relaxing.
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

// Returns how much light reaches the point, not whether anything blocked it.
// The closest the ray passes to a surface, relative to how far it has gone, is
// an approximate cone intersection and costs nothing beyond the march we were
// doing anyway - which is where soft shadows come from for free. Owns nothing
// and answers one question (idiom 23), and stops early once it is fully dark.
float lightReaching(vec3 origin, vec3 direction) {
  float result = 1.0;
  float distanceTravelled = surfaceBias();

  for (int i = 0; i < int(params.maxIterations); ++i) {
    float distanceToSurface =
      fieldDistance(origin + direction * distanceTravelled).x;

    if (distanceToSurface <= params.epsilon) {
      return 0.0;
    }

    result =
      min(result,
          params.shadowSharpness * distanceToSurface / distanceTravelled);

    distanceTravelled += distanceToSurface;

    if (distanceTravelled > params.shadowMaxDistance || result < 0.01) {
      break;
    }
  }

  return clamp(result, 0.0, 1.0);
}

vec3 albedo(float materialId) {
  return materialTable.materials[uint(materialId + 0.5)].baseColor.rgb;
}

void main() {
  vec4 clipPosition = vec4(ndc.xy, 1.0, 1.0);

  vec4 worldPosition = camera.inverseViewProj * clipPosition;

  worldPosition /= worldPosition.w;

  vec3 worldDirection = normalize(worldPosition.xyz - camera.position.xyz);
  vec3 localDirection =
    (fieldObject.inverseModel * vec4(worldDirection, 0)).xyz;

  vec3 localCameraPos =
    (fieldObject.inverseModel * vec4(camera.position.xyz, 1)).xyz;

  vec3 localHitPosition;
  float materialId;
  if (march(localCameraPos, localDirection, localHitPosition, materialId)) {
    vec3 surfaceAlbedo = albedo(materialId);

    vec4 worldHitPos = fieldObject.model * vec4(localHitPosition, 1.0);
    vec4 clip = camera.viewProj * worldHitPos;

    float depth = clip.z / clip.w;
    if (depth <= 0 || isinf(depth) || isnan(depth)) {
      discard;
    }
    gl_FragDepth = depth;

    vec3 worldLightDir = normalize(vec3(0.4, 1.0, 0.6));
    vec3 localLightDir =
      (fieldObject.inverseModel * vec4(worldLightDir, 0.0)).xyz;

    vec3 localNormal = estimateNormal(localHitPosition);

    // The gradient points out of solid geometry, which is away from the eye
    // when the surface is being viewed from inside. Face it back at the
    // viewer so an interior wall is shaded rather than left black.
    if (dot(localNormal, localDirection) > 0.0) {
      localNormal = -localNormal;
    }

    float diffuse = max(0.0, dot(localNormal, localLightDir));

    float light =
      diffuse > 0.0
        ? lightReaching(localHitPosition + localNormal * surfaceBias(),
                        localLightDir)
        : 1.0;

    vec3 color = vec3(surfaceAlbedo * (ambient + diffuse * light));
    outColor = vec4(color, 1.0);
  } else {
    discard;
  }
}
