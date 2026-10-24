// /*
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

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out mat3 outTBN;       // mat3 takes locations 3, 4, 5
layout(location = 6) out vec4 outCurNDC;
layout(location = 7) out vec4 outPrevNDC;
layout(location = 8) flat out uint outObjIdx;

invariant gl_Position;  // must match depthPrepass.vert bit for bit for the depth test

layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
} pc;

void main() {
    Object obj = pc.objRef.objects[gl_InstanceIndex];
    Vertex vert = fetchVertex(obj, gl_VertexIndex);

    mat4 skinMatrix = getSkinMatrix(pc.boneTableRef, obj.boneMatricesIdx, vert);
    vec4 localPosition = skinMatrix * vec4(vert.pos, 1.0);
    vec4 worldPos = obj.model * localPosition;    // localPosition is already skinned
    outWorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(obj.model * skinMatrix)));
    vec3 T = normalize(normalMatrix * vert.tangent);
    vec3 B = normalize(normalMatrix * vert.biTangent);
    vec3 N = normalize(normalMatrix * vert.normal);
    outTBN = mat3(T, B, N);
    outNormal = N;
    outTexCoord = vert.uv;
    outObjIdx = gl_InstanceIndex;

    gl_Position = ubo.proj * ubo.view * worldPos;

    // uses this frame's pose: last frame's bone matrices are not kept, so bone motion is missing from the motion vector
    vec4 prevWorldPos = obj.prevModel * localPosition;
    outCurNDC = gl_Position;
    outPrevNDC = ubo.prevViewProj * prevWorldPos;
}