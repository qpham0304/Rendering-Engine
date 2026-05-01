#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require


layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

struct RayPayload {
    vec3 hitPos;
    vec3 bc;
    int instanceIndex;
    int primitiveIndex;
    uint hit;
    uint seed;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

struct Material {
    uint albedoIdx; uint normalIdx; uint metalnessIdx; uint roughnessIdx;
    uint aoIdx; uint emissiveIdx; vec2 uvOffset; vec4 albedoFactor;
    vec4 normalFactor; float metallicFactor; float roughnessFactor;
    float aoFactor; float emissiveFactor;
};

struct Vertex {
    vec3 pos; vec3 col; vec2 uv; vec3 normal; vec3 tangent; vec3 biTangent;
    int m_BoneIDs[4]; float m_Weights[4];
};

struct Object {
    uint64_t vertexAddress; uint64_t indexAddress;
    uint64_t materialsRef; uint64_t materialIndiciesRef;
};

layout(buffer_reference, scalar) buffer ObjectsBuffer { Object objects[]; };
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { uint i[]; };
layout(buffer_reference, scalar) buffer MaterialsBuffer { Material m[]; };
layout(buffer_reference, scalar) readonly buffer MatIndicesBuffer { uint i[]; };

layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    uint objIdx;
} pc;

void main() {
    // get the Object info for the hit instance
    Object obj = pc.objRef.objects[gl_InstanceCustomIndexEXT];
    
    // fetch the indices for the triangle
    Indices indices = Indices(obj.indexAddress);
    uint i0 = indices.i[3 * gl_PrimitiveID + 0];
    uint i1 = indices.i[3 * gl_PrimitiveID + 1];
    uint i2 = indices.i[3 * gl_PrimitiveID + 2];

    // fetch vertices to get UVs or Normals
    Vertices vertices = Vertices(obj.vertexAddress);
    vec2 uv0 = vertices.v[i0].uv;
    vec2 uv1 = vertices.v[i1].uv;
    vec2 uv2 = vertices.v[i2].uv;

    // interpolate UVs using barycentrics
    vec3 bc = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec2 hitUV = uv0 * bc.x + uv1 * bc.y + uv2 * bc.z;

    MatIndicesBuffer matIndices = MatIndicesBuffer(obj.materialIndiciesRef);
    uint materialID = matIndices.i[gl_PrimitiveID];

    MaterialsBuffer allMaterials = MaterialsBuffer(obj.materialsRef);
    Material mat = allMaterials.m[materialID];
    vec3 textureColor = texture(samplerImages[nonuniformEXT(mat.albedoIdx)], hitUV).rgb;

    vec3 finalAlbedo = textureColor * mat.albedoFactor.rgb;

    payload.hit = 1;
    vec3 v0 = vertices.v[i0].pos;
    vec3 v1 = vertices.v[i1].pos;
    vec3 v2 = vertices.v[i2].pos;
    vec3 worldNormal = normalize(cross(v1 - v0, v2 - v0));

    float dotNL = max(dot(worldNormal, -gl_WorldRayDirectionEXT), 0.2); // 0.2 is ambient floor

    payload.hitPos = finalAlbedo * dotNL;
}