#version 450

layout(std140, set = 0, binding = 0) uniform CameraUniform {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
  mat4 inverseViewProj;
  vec4 position;
} camera;

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

layout(push_constant) uniform PushConstants {
  mat4 model;
  uint materialIndex;
  uint objectIndex;
} push;

layout(location = 0) out vec4 clipPosition;

// Temporary Step 4 fixture.
// Removed once all field objects, including the ground, are finite.
const float PROXY_INFLATE = 1000.0;

// Eight possible box corners:
//
// 0 = min.x, min.y, min.z
// 1 = max.x, min.y, min.z
// 2 = min.x, max.y, min.z
// 3 = max.x, max.y, min.z
// 4 = min.x, min.y, max.z
// 5 = max.x, min.y, max.z
// 6 = min.x, max.y, max.z
// 7 = max.x, max.y, max.z
//
// Six faces, two triangles per face, three vertices per triangle.
const uint CUBE_INDICES[36] = uint[](
  // -Z
  0,
  2,
  1,
  1,
  2,
  3,

  // +Z
  4,
  5,
  6,
  5,
  7,
  6,

  // -X
  0,
  4,
  2,
  4,
  6,
  2,

  // +X
  1,
  3,
  5,
  5,
  3,
  7,

  // -Y
  0,
  1,
  4,
  1,
  5,
  4,

  // +Y
  2,
  6,
  3,
  3,
  6,
  7);

void main() {
  FieldObjectShared object = fieldObjectTable.objects[push.objectIndex];

  // The CPU has already derived the grid. localOrigin is its minimum
  // lattice point, and N lattice points span N - 1 cells.
  vec3 boxMin = object.localOrigin.xyz;

  vec3 span =
    object.voxelSize.xyz * vec3(object.resolutionVolumeIndex.xyz - 1u);

  vec3 boxMax = boxMin + span;

  // Step 4 only: keep the current unbounded ground visible while validating
  // the proxy draw path. Inflate around the existing grid's centre.
  vec3 center = 0.5 * (boxMin + boxMax);
  vec3 halfExtent = 0.5 * (boxMax - boxMin) * PROXY_INFLATE;

  boxMin = center - halfExtent;
  boxMax = center + halfExtent;

  uint corner = CUBE_INDICES[gl_VertexIndex];

  vec3 localPosition = vec3((corner & 1u) != 0u ? boxMax.x : boxMin.x,
                            (corner & 2u) != 0u ? boxMax.y : boxMin.y,
                            (corner & 4u) != 0u ? boxMax.z : boxMin.z);

  clipPosition = camera.viewProj * object.model * vec4(localPosition, 1.0);

  gl_Position = clipPosition;
}
