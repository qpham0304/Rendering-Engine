#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_query : require

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