#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct RayPayload {
    vec3 hitPos;
    vec3 bc;
    int instanceIndex;
    int primitiveIndex;
    uint hit;
    uint seed;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (unitDir.y + 1.0);
    vec3 skyColor = (1.0 - t) * vec3(1.0) + t * vec3(0.5, 0.7, 1.0);
    
    payload.hitPos = skyColor; // reuse hitPos to pass back background color
    payload.hit = 0;
}