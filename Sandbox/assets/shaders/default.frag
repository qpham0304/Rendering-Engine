#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;

// layout(set = 0, binding = 0) uniform sampler2D textureSampler;

layout(set = 0, binding = 0) uniform sampler2D globalTextures[];

layout(location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstant {
    int index;
} pc;

void main() {
    // outColor = texture(textureSampler, inUV);
    outColor = texture(globalTextures[pc.index], inUV);
}
