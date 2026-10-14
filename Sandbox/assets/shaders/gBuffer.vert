// /*
#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;
layout(location = 6) in ivec4 inBoneIDs;
layout(location = 7) in vec4 inBoneWeights;

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

layout(set = 0, binding = 1, std430) readonly buffer StorageBufferObject {
    mat4 models[];
} ssbo;

layout(set = 0, binding = 2, std430) readonly buffer PreviousModels {
    mat4 models[];
} prevSsbo;


layout(buffer_reference, scalar) buffer BoneModelBuffer {
    mat4 boneMatrices[];
};

layout(buffer_reference, scalar) buffer BoneMatrixArrayTable {
    BoneModelBuffer ptrs[];
};

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out mat3 outTBN;       // mat3 takes locations 3, 4, 5
layout(location = 6) out vec4 outCurNDC;
layout(location = 7) out vec4 outPrevNDC;


layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
    uint objIdx;
    uint boneMatricesIdx;
} pc;

void main() {
    mat4 skinMatrix;
    if(pc.boneMatricesIdx == 0) {
        skinMatrix = mat4(1.0);
    } else {
        BoneModelBuffer modelBones = pc.boneTableRef.ptrs[pc.boneMatricesIdx];
        skinMatrix  = (inBoneIDs.x >= 0 ? modelBones.boneMatrices[inBoneIDs.x] * inBoneWeights.x : mat4(0.0))
                    + (inBoneIDs.y >= 0 ? modelBones.boneMatrices[inBoneIDs.y] * inBoneWeights.y : mat4(0.0))
                    + (inBoneIDs.z >= 0 ? modelBones.boneMatrices[inBoneIDs.z] * inBoneWeights.z : mat4(0.0))
                    + (inBoneIDs.w >= 0 ? modelBones.boneMatrices[inBoneIDs.w] * inBoneWeights.w : mat4(0.0));
    }

    vec4 localPosition = skinMatrix * vec4(inPosition, 1.0);
    mat4 modelMatrix = ssbo.models[gl_InstanceIndex];
    vec4 worldPos = modelMatrix * localPosition;    // localPosition is already skinned
    outWorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix * skinMatrix)));
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 B = normalize(normalMatrix * inBiTangent);
    vec3 N = normalize(normalMatrix * inNormal);
    outTBN = mat3(T, B, N);
    outNormal = N;
    outTexCoord = inTexCoord;

    gl_Position = ubo.proj * ubo.view * worldPos;

    mat4 prevModelMatrix = prevSsbo.models[gl_InstanceIndex]; 
    // uses this frame's pose: last frame's bone matrices are not kept, so bone motion is missing from the motion vector
    vec4 prevWorldPos = prevModelMatrix * localPosition;
    outCurNDC = gl_Position;
    outPrevNDC = ubo.prevViewProj * prevWorldPos;
}
// */

// working vertex pulling but performance is horrible
/*
#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

const uint MAX_BONE_INFLUENCE =  4;
struct Vertex
{
    vec3 pos;
    vec3 col;
    vec2 uv;
    vec3 normal;
    vec3 tangent;
    vec3 biTangent;
    int m_BoneIDs[MAX_BONE_INFLUENCE];
    float m_Weights[MAX_BONE_INFLUENCE];
};

struct Object {
    uint64_t vertexAddress;
    uint64_t indexAddress;
    uint64_t materialsRef;
    uint64_t materialIndiciesRef;
};

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { uint i[]; };
layout(buffer_reference, scalar) buffer ObjectsBuffer { Object objects[]; };

// Push constant to get the pointer to our object list
layout(push_constant) uniform PushConstants {
    ObjectsBuffer objRef;
    uint objIdx; // The index for THIS draw call
} pc;


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

layout(set = 0, binding = 1, std430) readonly buffer StorageBufferObject {
    mat4 models[];
} ssbo;

layout(set = 0, binding = 2, std430) readonly buffer PreviousModels {
    mat4 models[];
} prevSsbo;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out mat3 outTBN;
layout(location = 6) out vec4 outCurNDC;
layout(location = 7) out vec4 outPrevNDC;

void main() {
    Object obj = pc.objRef.objects[pc.objIdx];
    Vertices vertices = Vertices(obj.vertexAddress);
    Indices indices = Indices(obj.indexAddress);

    // programmable Vertex Fetching
    // gl_VertexIndex is the magic variable here
    uint idx = indices.i[gl_VertexIndex];
    Vertex vert = vertices.v[idx];

    // transformations using the fetched vert instead of inPosition
    mat4 modelMatrix = ssbo.models[gl_InstanceIndex];
    vec4 worldPos = modelMatrix * vec4(vert.pos, 1.0);
    outWorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    vec3 T = normalize(normalMatrix * vert.tangent);
    vec3 B = normalize(normalMatrix * vert.biTangent);
    vec3 N = normalize(normalMatrix * vert.normal);
    outTBN = mat3(T, B, N);
    outNormal = N;
    outTexCoord = vert.uv;

    gl_Position = ubo.proj * ubo.view * worldPos;

    mat4 prevModelMatrix = prevSsbo.models[gl_InstanceIndex]; 
    vec4 prevWorldPos = prevModelMatrix * vec4(vert.pos, 1.0);
    outCurNDC = gl_Position;
    outPrevNDC = ubo.prevViewProj * prevWorldPos;
}
*/
