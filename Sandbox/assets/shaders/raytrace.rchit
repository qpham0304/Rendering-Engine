#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Define the payload (must match location in RayGen)
layout(location = 0) rayPayloadInEXT vec3 hitColor;


hitAttributeEXT vec2 attribs;   // Built-in barycentric coordinates

void main() {
    // just visualize the Geometry Normal provided by the ray tracing hardware.
    vec3 normal = gl_WorldRayDirectionEXT;
    
    // orientation check
    hitColor = normalize(gl_ObjectRayDirectionEXT) * 0.5 + 0.5;
}