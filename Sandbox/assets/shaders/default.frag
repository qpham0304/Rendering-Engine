#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D textureSampler;


layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(textureSampler, inUV);
    // outColor = vec4(1.0, 1.0, 0.0, 1.0);
}
