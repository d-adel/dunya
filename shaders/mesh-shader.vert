#version 450

layout(set = 0, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 inverseViewProj;
    vec4 position;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;

layout (push_constant) uniform constants
{
  mat4 model;
  uint materialIndex;
} PushConstants;

void main() {
    gl_Position = camera.proj * camera.view * PushConstants.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal =  mat3(PushConstants.model) * inNormal;
}
