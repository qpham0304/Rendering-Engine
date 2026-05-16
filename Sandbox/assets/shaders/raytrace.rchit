#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require

struct RayPayload {
    vec3 hitPos;
    vec3 worldNormal;
    vec3 bc;
    int instanceIndex;
    int primitiveIndex;
    uint hit;
    uint seed;
    uint isShadowed;
};

struct Material {
    uint albedoIdx;
    uint normalIdx;
    uint metalnessIdx;
    uint roughnessIdx;
    uint aoIdx;
    uint emissiveIdx;

    vec2 uvOffset;
    vec4 albedoFactor;
    vec4 normalFactor;
    float metallicFactor;
    float roughnessFactor;
    float aoFactor;
    float emissiveFactor;
};

const uint MAX_BONE_INFLUENCE =  4;
const float PI = 3.14159265359;

struct Vertex {
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


layout(buffer_reference, scalar) buffer ObjectsBuffer { Object objects[]; };
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { uint i[]; };
layout(buffer_reference, scalar) buffer MaterialsBuffer { Material m[]; };
layout(buffer_reference, scalar) readonly buffer MatIndicesBuffer { uint i[]; };


layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    uint objIdx;
    uint bluenoiseIdx;
    uint explicitPass;
} pc;

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

void main() {
    Object obj = pc.objRef.objects[gl_InstanceCustomIndexEXT];
    Vertices vertices = Vertices(obj.vertexAddress);
    Indices indices = Indices(obj.indexAddress);
    MatIndicesBuffer matIndices = MatIndicesBuffer(obj.materialIndiciesRef);

    uint i0 = indices.i[3 * gl_PrimitiveID + 0];
    uint i1 = indices.i[3 * gl_PrimitiveID + 1];
    uint i2 = indices.i[3 * gl_PrimitiveID + 2];

    vec3 bc = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 nrmObject = bc.x * vertices.v[i0].normal + 
                     bc.y * vertices.v[i1].normal + 
                     bc.z * vertices.v[i2].normal;
    
    // payload.matIdx = matIndices.i[gl_PrimitiveID];

    payload.hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    payload.worldNormal = normalize(nrmObject * gl_ObjectToWorldEXT).xyz;
    payload.instanceIndex = gl_InstanceCustomIndexEXT;
    payload.primitiveIndex = gl_PrimitiveID;
    payload.bc = bc;
    payload.hit = 1;
}