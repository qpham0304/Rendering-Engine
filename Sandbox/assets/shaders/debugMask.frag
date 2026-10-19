#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 0) out vec4 outKind;

layout (push_constant) uniform PushConstant {
    mat4 model;
    float kind;     // 0.5 = hover, 1.0 = select
} pc;

void main() {
    outKind = vec4(pc.kind, 0.0, 0.0, 1.0);
}
