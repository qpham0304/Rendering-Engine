#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedoMaps;
layout(set = 1, binding = 1) uniform sampler2D normalMaps;
layout(set = 1, binding = 2) uniform sampler2D metalnessMaps;
layout(set = 1, binding = 3) uniform sampler2D roughnessMaps;
layout(set = 1, binding = 4) uniform sampler2D aoMaps;
layout(set = 1, binding = 5) uniform sampler2D emissiveMaps;

layout(push_constant) uniform PushConstantData {
    vec3 color;
    vec3 range;
    bool flag;
    float data;
} pushConstantData;

void main() {
    if (pushConstantData.flag) {
        outColor = texture(albedoMaps, fragTexCoord);
    } else {
        outColor = texture(normalMaps, fragTexCoord);
        // outColor = vec4(pushConstantData.color * pushConstantData.data, 1.0f);
    }
    outColor = vec4(pow(outColor.xyz, vec3(1.0f/2.2f)), 1.0f);
}