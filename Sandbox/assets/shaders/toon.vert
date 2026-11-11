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
    vec2 jitter;
    float mipBias;
} ubo;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) flat out uint outObjIdx;

invariant gl_Position;  // must match depthPrepass.vert bit for bit for the depth test

layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 shadowTint;
    vec4 params;
} pc;

void main() {
    Object obj = pc.objRef.objects[gl_InstanceIndex];
    Vertex vert = fetchVertex(obj, gl_VertexIndex);

    mat4 skinMatrix = getSkinMatrix(pc.boneTableRef, obj.boneMatricesIdx, vert);
    vec4 localPosition = skinMatrix * vec4(vert.pos, 1.0);
    vec4 worldPos = obj.model * localPosition;

    gl_Position = ubo.proj * ubo.view * worldPos;

    mat3 normalMatrix = transpose(inverse(mat3(obj.model * skinMatrix)));
    outNormal = normalize(normalMatrix * vert.normal);
    outTexCoord = vert.uv;
    outWorldPos = worldPos.xyz;
    outObjIdx = gl_InstanceIndex;
}
