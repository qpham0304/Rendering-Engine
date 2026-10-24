#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
    mat4 prevViewProj;
    mat4 proj;
    vec4 cameraPos;
    mat4 invView;
    mat4 invProj;
    float width;
    float height;
} ubo;

layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
} pc;

invariant gl_Position;

void main() {
    Object obj = pc.objRef.objects[gl_InstanceIndex];
    Vertex vert = fetchVertex(obj, gl_VertexIndex);

    mat4 skinMatrix = getSkinMatrix(pc.boneTableRef, obj.boneMatricesIdx, vert);
    vec4 localPosition = skinMatrix * vec4(vert.pos, 1.0);
    vec4 worldPos = obj.model * localPosition;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
