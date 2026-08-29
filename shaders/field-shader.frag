#version 450
#extension GL_GOOGLE_include_directive : require

const int MAX_MATERIALS = DUNYA_MAX_MATERIALS;
const int MAX_FIELD_RECORDS = DUNYA_MAX_FIELD_RECORDS;
const int MAX_FIELD_VOLUMES = DUNYA_MAX_FIELD_VOLUMES;
const uint BRICK_CELLS = DUNYA_BRICK_CELLS;
const float bias = 0.01;
const float ambient = 0.06;

layout(std140, set = 0, binding = 1) uniform MarchParams {
  float epsilon;
  float maxDistance;
  float omega;

  float gradientEpsilon;
  float shadowMaxDistance;
  float shadowSharpness;
  uint maxIterations;
} params;

// How many record slots this frame actually filled. The table is a fixed 64;
// the count is what says where the real ones stop.
layout(std140, set = 0, binding = 2) uniform SceneCounts {
  uint fieldRecords;
} counts;

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

layout(std140, set = 2, binding = 0) readonly buffer FieldRecordTable {
  FieldRecordShared records[];
} fieldRecordTable;

const int MAX_SAMPLERS = DUNYA_MAX_SAMPLERS;

layout(set = 1, binding = 2) uniform sampler samplers[MAX_SAMPLERS];

layout(set = 2, binding = 1)
  uniform texture3D distanceVolume[MAX_FIELD_VOLUMES];

layout(set = 2, binding = 2)
  uniform utexture3D materialVolume[MAX_FIELD_VOLUMES];

// Written by field-lipschitz.comp after each bake. One fixed slot per volume,
// so localOrigin.w is where an object's own bounds begin.
layout(std430, set = 2, binding = 6) readonly buffer BrickBounds {
  float values[];
} brickBounds;

layout(location = 0) in vec4 clipPosition;
layout(location = 1) flat in uint recordIndex;

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

float outsideGrid(FieldRecordShared record, vec3 p) {
  vec3 maxCorner = record.localOrigin.xyz
                   + record.voxelSize.xyz
                       * vec3(record.resolutionVolumeIndex.xyz - 1u);

  return length(p - clamp(p, record.localOrigin.xyz, maxCorner));
}

vec2 gridSample(FieldRecordShared record, vec3 p) {
  uint volumeIndex = record.resolutionVolumeIndex.w;

  vec3 lattice = (p - record.localOrigin.xyz) / record.voxelSize.xyz;

  vec3 uvw = (lattice + 0.5) / vec3(record.resolutionVolumeIndex.xyz);

  float distance = texture(sampler3D(distanceVolume[volumeIndex],
                                     samplers[DUNYA_SAMPLER_LINEAR_CLAMP]),
                           uvw)
                     .r;

  uint material = texture(usampler3D(materialVolume[volumeIndex],
                                     samplers[DUNYA_SAMPLER_NEAREST_CLAMP]),
                          uvw)
                    .r;

  return vec2(distance, float(material));
}

// The bound the reduction pass measured for the brick this point sits in. It
// bounds the interpolant's own gradient, so |g| / bound is a distance the ray
// can travel without reaching the zero surface.
float brickBound(FieldRecordShared record, vec3 p) {
  uvec3 cells = record.resolutionVolumeIndex.xyz - 1u;
  uvec3 brickResolution = (cells + BRICK_CELLS - 1u) / BRICK_CELLS;

  vec3 lattice = (p - record.localOrigin.xyz) / record.voxelSize.xyz;

  uvec3 cell = min(uvec3(max(floor(lattice), vec3(0.0))), cells - 1u);
  uvec3 brick = min(cell / BRICK_CELLS, brickResolution - 1u);

  uint index =
    brick.x + brickResolution.x * (brick.y + brickResolution.y * brick.z);

  return brickBounds.values[uint(record.localOrigin.w) + 1u + index];
}

// How far the ray may go before leaving the brick whose bound it just read.
// Beyond that wall the next brick may be steeper, and the same step would be
// too long there.
float brickExit(FieldRecordShared record, vec3 p, vec3 direction) {
  uvec3 cells = record.resolutionVolumeIndex.xyz - 1u;

  vec3 lattice = (p - record.localOrigin.xyz) / record.voxelSize.xyz;

  uvec3 cell = min(uvec3(max(floor(lattice), vec3(0.0))), cells - 1u);
  uvec3 base = (cell / BRICK_CELLS) * BRICK_CELLS;

  vec3 minimum =
    record.localOrigin.xyz + record.voxelSize.xyz * vec3(base);
  vec3 maximum = record.localOrigin.xyz
                 + record.voxelSize.xyz
                     * vec3(min(base + BRICK_CELLS, cells));

  float exit = params.maxDistance;

  for (int axis = 0; axis < 3; ++axis) {
    if (abs(direction[axis]) < 1e-8) {
      continue;
    }

    float wall = direction[axis] > 0.0 ? maximum[axis] : minimum[axis];

    exit = min(exit, (wall - p[axis]) / direction[axis]);
  }

  // A ray next to a wall has almost no room, and stepping that gap never leaves
  // the brick. Half a voxel is the floor: enough to cross, short enough that
  // the bound still describes it.
  vec3 voxel = record.voxelSize.xyz;

  return max(exit, 0.5 * min(voxel.x, min(voxel.y, voxel.z)));
}

vec2 fieldDistance(FieldRecordShared record, vec3 p) {
  if (record.config.y == 0u) {
    fieldPrimitiveOffset = record.config.w;
    fieldPrimitiveCount = record.config.x;
    return sceneDistance(p);
  }

  float outside = outsideGrid(record, p);

  if (outside > 0.0) {
    return vec2(outside + record.voxelSize.w, 0.0);
  }

  return gridSample(record, p);
}

// What the field says and how far the ray may act on it are two things; they
// differ only for the sampled representation. Both functions below return a
// step no zero of the field lies inside.
float fieldStep(FieldRecordShared record,
                vec3 p,
                vec3 direction,
                float value) {
  if (record.config.y == 0u) {
    return abs(value);
  }

  // Outside the grid the value already is a bound: the distance to the box.
  if (outsideGrid(record, p) > 0.0) {
    return abs(value);
  }

  float bound = brickBound(record, p);
  float exit = brickExit(record, p, direction);

  if (bound <= 0.0) {
    return exit;
  }

  return min(abs(value) / bound, exit);
}

// The bound over the whole grid rather than one brick's: looser, so shorter
// steps, but it needs no wall to stop at. Clamping to brick walls would
// quantise where a soft shadow's samples fall.
float fieldShadowStep(FieldRecordShared record, vec3 p, float value) {
  if (record.config.y == 0u) {
    return abs(value);
  }

  if (outsideGrid(record, p) > 0.0) {
    return abs(value);
  }

  float bound = brickBounds.values[uint(record.localOrigin.w)];

  if (bound <= 0.0) {
    return abs(value);
  }

  return abs(value) / bound;
}

float gradientOffset(FieldRecordShared record) {
  if (record.config.y == 0u) {
    return params.gradientEpsilon;
  }

  vec3 voxel = record.voxelSize.xyz;

  return max(params.gradientEpsilon, max(voxel.x, max(voxel.y, voxel.z)));
}

float surfaceBias(FieldRecordShared record) {
  if (record.config.y == 0u) {
    return bias;
  }

  vec3 voxel = record.voxelSize.xyz;

  return max(bias, 2.0 * max(voxel.x, max(voxel.y, voxel.z)));
}

vec3 estimateNormal(FieldRecordShared record, vec3 p) {
  float h = gradientOffset(record);

  vec3 offsetX = vec3(h, 0.0, 0.0);
  vec3 offsetY = vec3(0.0, h, 0.0);
  vec3 offsetZ = vec3(0.0, 0.0, h);

  return normalize(vec3(fieldDistance(record, p + offsetX).x
                          - fieldDistance(record, p - offsetX).x,

                        fieldDistance(record, p + offsetY).x
                          - fieldDistance(record, p - offsetY).x,

                        fieldDistance(record, p + offsetZ).x
                          - fieldDistance(record, p - offsetZ).x));
}

bool march(FieldRecordShared record,
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

    vec2 field = fieldDistance(record, p);

    if (i == 0) {
      functionSign = field.x < 0.0 ? -1.0 : 1.0;
    }

    float signedRadius = functionSign * field.x;

    // The bound the ray may travel on, which for the analytic field is the
    // distance itself. Stepping and the relaxation test both need a bound; the
    // hit test below wants the field's real value.
    float radius = fieldStep(record, p, direction, field.x);

    bool relaxationFailed =
      omega > 1.0 && (radius + previousRadius) < stepLength;

    if (relaxationFailed) {
      stepLength -= omega * stepLength;
      omega = 1.0;
    } else {
      stepLength = (signedRadius < 0.0 ? -radius : radius) * omega;
    }

    previousRadius = radius;

    if (!relaxationFailed && abs(signedRadius) <= params.epsilon) {
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

float lightReachingLocal(FieldRecordShared record,
                         vec3 origin,
                         vec3 direction) {
  float result = 1.0;
  float distanceTravelled = 0.0;

  for (int i = 0; i < int(params.maxIterations); ++i) {
    vec3 p = origin + direction * distanceTravelled;

    bool trueDistance = true;
    float distanceToSurface;
    float step;

    if (record.config.y == 0u) {
      // Analytic field: fieldDistance is meaningful everywhere.
      distanceToSurface = fieldDistance(record, p).x;
      step = distanceToSurface;
    } else {
      float outside = outsideGrid(record, p);

      if (outside > 0.0) {
        // Outside a sampled object's grid this is only a conservative
        // marching bound to the grid, not distance to actual geometry.
        distanceToSurface = outside + record.voxelSize.w;
        step = distanceToSurface;

        trueDistance = false;
      } else {
        // Inside the grid we have an actual sampled field distance, and a
        // separate bound saying how far the ray may act on it.
        distanceToSurface = fieldDistance(record, p).x;
        step = fieldShadowStep(record, p, distanceToSurface);
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

    distanceTravelled += step;

    if (distanceTravelled > params.shadowMaxDistance || result < 0.01) {
      break;
    }
  }

  return clamp(result, 0.0, 1.0);
}

// Whether the segment [0, maxDistance] along the ray reaches this record's
// grid box at all. The slab test, in the record's own local space, where the
// box is axis aligned and no transform of the box is needed.
//
// This is what makes a shadow affordable with a hundred bodies in the scene.
// Without it every shaded pixel sphere-traces every record for up to
// maxIterations steps, whether or not the light path goes anywhere near it -
// the cost is bodies times pixels, and it was measured at about a tenth of a
// millisecond per body. A shadow ray crosses two or three objects; the slab
// test costs a few instructions and removes the rest.
bool shadowRayReaches(FieldRecordShared record,
                      vec3 localOrigin,
                      vec3 localDirection,
                      float maxDistance) {
  vec3 minCorner = record.localOrigin.xyz;
  vec3 maxCorner = minCorner
                   + record.voxelSize.xyz
                       * vec3(record.resolutionVolumeIndex.xyz - 1u);

  // A component of exactly zero gives an infinity here and the min/max below
  // handle it correctly; a NaN would not, which is why the direction is never
  // renormalised into something that could be zero length.
  vec3 inverseDirection = 1.0 / localDirection;

  vec3 firstPlane = (minCorner - localOrigin) * inverseDirection;
  vec3 secondPlane = (maxCorner - localOrigin) * inverseDirection;

  vec3 nearPlane = min(firstPlane, secondPlane);
  vec3 farPlane = max(firstPlane, secondPlane);

  float entry = max(max(nearPlane.x, nearPlane.y), nearPlane.z);
  float exitAt = min(min(farPlane.x, farPlane.y), farPlane.z);

  // Behind the origin is still a miss for a shadow, and so is beyond the
  // distance the march would have given up at anyway.
  return exitAt >= max(entry, 0.0) && entry <= maxDistance;
}

float lightReaching(vec3 worldOrigin, vec3 worldDirection) {
  float result = 1.0;

  // Bounded by the count, not the table's capacity. Nothing clears a slot, so
  // one left from a busier frame would still claim to be live.
  for (uint i = 0u; i < counts.fieldRecords; ++i) {
    FieldRecordShared record = fieldRecordTable.records[i];

    // Same world -> local crossing as the primary ray: point w = 1, direction
    // w = 0. Do not renormalise the direction.
    vec3 localOrigin = (record.inverseModel * vec4(worldOrigin, 1.0)).xyz;

    vec3 localDirection = (record.inverseModel * vec4(worldDirection, 0.0)).xyz;

    if (!shadowRayReaches(record,
                          localOrigin,
                          localDirection,
                          params.shadowMaxDistance)) {
      continue;
    }

    float recordLight = lightReachingLocal(record, localOrigin, localDirection);

    result = min(result, recordLight);

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
  FieldRecordShared record = fieldRecordTable.records[recordIndex];

  vec2 ndc = clipPosition.xy / clipPosition.w;

  vec4 worldPosition = camera.inverseViewProj * vec4(ndc, 1.0, 1.0);

  worldPosition /= worldPosition.w;

  vec3 worldDirection = normalize(worldPosition.xyz - camera.position.xyz);

  vec3 localDirection =
    (record.inverseModel * vec4(worldDirection, 0.0)).xyz;

  vec3 localCameraPos =
    (record.inverseModel * vec4(camera.position.xyz, 1.0)).xyz;

  vec3 localHitPosition;
  float materialId;

  if (!march(record,
             localCameraPos,
             localDirection,
             localHitPosition,
             materialId)) {
    discard;
  }

  vec3 surfaceAlbedo = albedo(materialId);

  vec4 worldHitPos = record.model * vec4(localHitPosition, 1.0);

  vec4 clip = camera.viewProj * worldHitPos;

  float depth = clip.z / clip.w;

  if (depth <= 0.0 || isinf(depth) || isnan(depth)) {
    discard;
  }

  gl_FragDepth = depth;

  vec3 localNormal = estimateNormal(record, localHitPosition);

  // Face the normal toward the viewer for interior surfaces.
  if (dot(localNormal, localDirection) > 0.0) {
    localNormal = -localNormal;
  }

  // Rigid transform, so the model's rotational part is sufficient.
  vec3 worldNormal =
    normalize((record.model * vec4(localNormal, 0.0)).xyz);

  vec3 worldLightDir = normalize(vec3(0.4, 1.0, 0.6));

  float diffuse = max(0.0, dot(worldNormal, worldLightDir));

  float departureBias = surfaceBias(record);

  vec3 shadowOrigin = worldHitPos.xyz + worldNormal * departureBias;

  float light =
    diffuse > 0.0 ? lightReaching(shadowOrigin, worldLightDir) : 1.0;

  vec3 color = surfaceAlbedo * (ambient + diffuse * light);

  outColor = vec4(color, 1.0);
}
