#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) out vec2 outUV;

// a triangle to cover the entire screen
void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outUV = uv;
    
    gl_Position = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
}