#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBitangent;
layout(location = 6) flat out uint outObjIdx;

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

layout(buffer_reference, scalar) buffer Materials{  Material m[]; };

layout(push_constant) uniform PushConstant {
    Materials materialsRef;
    float bias;
    float time;
    mat4 sunlightMVP;
    vec4 direction;
    // vec4 color;
    float numLights;
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
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
    outTangent   = normalize(normalMatrix * vert.tangent);
    outBitangent = normalize(normalMatrix * vert.biTangent);

    fragColor = vert.col;
    fragTexCoord = vert.uv;
    fragWorldPos = worldPos.xyz;
    outObjIdx = gl_InstanceIndex;
}
