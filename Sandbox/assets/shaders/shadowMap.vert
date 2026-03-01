#version 460

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform MeshData {
    mat4 model;
    mat4 lightMVP;
} push;

void main() {
    gl_Position = push.lightMVP * push.model * vec4(inPosition, 1.0);
}