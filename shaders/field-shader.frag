#version 450

layout(push_constant) uniform PushConstantsBlock
{
    mat4 inverseViewProj;
    vec4 cameraPos;
} pushConstants;

layout(location = 0) in vec4 ndc;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 clipPosition = vec4(ndc.xy, 1.0, 1.0);

    vec4 worldPosition =
        pushConstants.inverseViewProj * clipPosition;

    worldPosition /= worldPosition.w;

    vec3 direction = normalize(
        worldPosition.xyz - pushConstants.cameraPos.xyz
    );

    outColor = vec4(direction * 0.5 + 0.5, 1.0);
}
