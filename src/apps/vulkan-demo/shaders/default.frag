#version 450

// layout(location = 0) in vec3 fragColor;

// layout(location = 0) out vec4 outColor;

// vec3 color = {0.0f, 1.0f, 1.0f};
// void main() {
//     outColor = vec4(fragColor, 1.0);
// }

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstantData {
    vec3 color;
    vec3 range;
    bool flag;
    float data;
} pushConstantData;

void main() {
    if (pushConstantData.flag) {
        outColor = texture(texSampler, fragTexCoord);
    } else {
        outColor = vec4(pushConstantData.color * pushConstantData.data, 1.0f);
    }
    outColor = vec4(pow(outColor.xyz, vec3(1.0f/2.2f)), 1.0f);
}