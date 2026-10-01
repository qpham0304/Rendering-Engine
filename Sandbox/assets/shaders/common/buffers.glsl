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
    uint64_t materialIndicesRef;
};

layout(buffer_reference, scalar) buffer ObjectsBuffer{ Object objects[]; };
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { uint i[]; };
layout(buffer_reference, scalar) buffer MaterialsBuffer{ Material m[]; };
layout(buffer_reference, scalar) readonly buffer MatIndicesBuffer { uint i[]; };

// Push constant to get the pointer to our object list
// layout(push_constant) uniform ObjectRefsPushConstants {
//     ObjectsBuffer objRef;
//     uint objIdx; // The index for THIS draw call
// } pc;

#endif