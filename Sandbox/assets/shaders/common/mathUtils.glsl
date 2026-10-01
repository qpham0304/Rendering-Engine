#ifndef MATH_UTILS_GLSL
#define MATH_UTILS_GLSL

#extension GL_GOOGLE_include_directive : require

#include "common/definitions.glsl"

vec3 getPosWorld(vec2 uv, float depth, mat4 invProj, mat4 invView) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = invProj * clip;
    viewPos /= viewPos.w;
    vec4 worldPos = invView * viewPos;
    return worldPos.xyz;
}

vec3 getPos(vec2 uv, float depth, mat4 invProj, mat4 invView) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = invProj * clip;
    viewPos /= viewPos.w;
    return viewPos.xyz;
}

#endif