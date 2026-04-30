#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

// Define the payload (must match location in RayGen)
layout(location = 0) rayPayloadInEXT vec3 hitColor;

// Built-in barycentric coordinates
hitAttributeEXT vec2 attribs;

void main() {
    // For now, let's just visualize the "Geometry Normal" 
    // provided by the ray tracing hardware.
    vec3 normal = gl_WorldRayDirectionEXT; // Fallback
    
    // Most basic visualization: Normal-to-Color mapping
    // This doesn't even require your vertex buffers to be set up correctly yet!
    hitColor = vec3(0.8, 0.2, 0.2); // Solid Red to verify hit
    
    // Or, if you want to see if the orientation is right:
    // hitColor = normalize(gl_ObjectRayDirectionEXT) * 0.5 + 0.5;
}