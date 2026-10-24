#ifndef COMMON_BUFFER_GLSL
#define COMMON_BUFFER_GLSL

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "definitions.glsl"
    
struct Material {
    uint albedoIdx;
    uint normalIdx;
    uint metalnessIdx;
    uint roughnessIdx;
    uint aoIdx;
    uint emissiveIdx;

    vec2 uvOffset;
    vec2 uvScale;

    vec4 albedoFactor;
    vec4 normalFactor;
    float metallicFactor;
    float roughnessFactor;
    float aoFactor;
    float emissiveFactor;
};


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

#define OBJECT_LIGHT_PROBE 1u

struct Object {
    uint64_t vertexAddress;
    uint64_t indexAddress;
    uint64_t materialsRef;
    uint64_t materialIndicesRef;
    mat4 model;
    mat4 prevModel;
    uint boneMatricesIdx;
    uint materialIdx;
    uint flags;
    uint padding;
};

layout(buffer_reference, scalar) buffer ObjectsBuffer{ Object objects[]; };
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { uint i[]; };
layout(buffer_reference, scalar) buffer MaterialsBuffer{ Material m[]; };
layout(buffer_reference, scalar) readonly buffer MaterialIndicesBuffer { uint i[]; };


layout(buffer_reference, scalar) buffer BoneModelBuffer { mat4 boneMatrices[]; };
layout(buffer_reference, scalar) buffer BoneMatrixArrayTable { BoneModelBuffer ptrs[]; };

// draws are non indexed, pass gl_VertexIndex
Vertex fetchVertex(Object obj, uint vertexIndex) {
    uint index = Indices(obj.indexAddress).i[vertexIndex];
    return Vertices(obj.vertexAddress).v[index];
}

mat4 getSkinMatrix(BoneMatrixArrayTable boneTable, uint boneMatricesIdx, Vertex vert) {
    if(boneMatricesIdx == 0) {
        return mat4(1.0);
    }

    BoneModelBuffer modelBones = boneTable.ptrs[boneMatricesIdx];
    mat4 skinMatrix = mat4(0.0);
    for(uint i = 0; i < MAX_BONE_INFLUENCE; i++) {
        if(vert.m_BoneIDs[i] >= 0) {
            skinMatrix += modelBones.boneMatrices[vert.m_BoneIDs[i]] * vert.m_Weights[i];
        }
    }
    return skinMatrix;
}

#endif