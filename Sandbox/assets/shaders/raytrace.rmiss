#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitColor;

void main() {
    // A simple blueish sky gradient based on ray direction
    vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (unitDir.y + 1.0);
    hitColor = (1.0 - t) * vec3(1.0) + t * vec3(0.5, 0.7, 1.0);
}