#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

// The scene light, shared with field-shader.frag: xyz toward it, w the ambient
// term. Only the direction is read here - this pass has its own ambient, which
// is a different number on purpose and not this block's business.
layout(std140, set = 0, binding = 3) uniform SceneLight {
  vec4 direction;
} light;

const int MAX_TEXTURES = DUNYA_MAX_TEXTURES;
const int MAX_SAMPLERS = DUNYA_MAX_SAMPLERS;
const int MAX_MATERIALS = DUNYA_MAX_MATERIALS;

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

layout(set = 1, binding = 1) uniform texture2D textures[MAX_TEXTURES];
layout(set = 1, binding = 2) uniform sampler samplers[MAX_SAMPLERS];

layout (push_constant) uniform constants
{
  mat4 model;
  uint materialIndex;
} PushConstants;

void main() {
    Material material = materialTable.materials[PushConstants.materialIndex];

    vec3 lightDir = light.direction.xyz;

    vec4 albedo = material.baseColor
        * texture(
            sampler2D(
                textures[material.baseColorTexture],
                samplers[material.baseColorSampler]
            ),
            fragTexCoord
        );
    vec3 ambient = vec3(0.005, 0.005, 0.005);
    vec3 diffuse = albedo.rgb * max(0.0, dot(normalize(fragNormal), lightDir));
    outColor = vec4(ambient + diffuse, 1);
}
