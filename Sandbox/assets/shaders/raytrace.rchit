#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require

struct RayPayload {
    vec3 hitPos;
    vec3 bc;
    int instanceIndex;
    int primitiveIndex;
    uint hit;
    uint seed;
    uint isShadowed;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

void main() {
    payload.hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    payload.instanceIndex = gl_InstanceCustomIndexEXT;
    payload.primitiveIndex = gl_PrimitiveID;
    payload.bc = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    payload.hit = 1;
}