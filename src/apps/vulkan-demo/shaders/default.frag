#version 450

// layout(location = 0) in vec3 fragColor;

// layout(location = 0) out vec4 outColor;

// vec3 color = {0.0f, 1.0f, 1.0f};
// void main() {
//     outColor = vec4(fragColor, 1.0);
// }

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstantData {
    vec3 color;
    vec3 range;
    bool flag;
    float data;
} pushConstantData;

void main() {
    if (pushConstantData.flag == true) {
        outColor = vec4(pushConstantData.range, 1.0f);
    } else {
        outColor = vec4(pushConstantData.color * pushConstantData.data, 1.0f);
    }

}