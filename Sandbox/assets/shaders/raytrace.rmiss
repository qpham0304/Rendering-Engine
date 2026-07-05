#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

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

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    payload.hit = 0;
    payload.isShadowed = 0;
}