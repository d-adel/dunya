#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.6));

    vec4 albedo = texture(texSampler, fragTexCoord);
    vec3 ambient = vec3(0.005, 0.005, 0.005);
    vec3 diffuse = albedo.rgb * max(0.0, dot(normalize(fragNormal), lightDir));
    outColor = vec4(ambient + diffuse, 1);
}
