#ifndef RAY_QUERY_LIGHTING_GLSL
#define RAY_QUERY_LIGHTING_GLSL

// lighting for surfaces found by a ray query (ddgi probe rays, ssr tlas hits), nothing on screen to read the light from.
// the including shader declares `topLevelAS` and the sky sh buffer `sh`, and enables GL_EXT_ray_query

#include "buffers.glsl"

// same SH the light pass uses for its diffuse ibl, carries a 1 / PI
vec3 getSkyIrradiance(vec3 N) {
    return max(vec3(0.0),
        0.886227 * sh.shCoeffs[0].xyz +
        1.023328 * sh.shCoeffs[1].xyz * N.y +
        1.023328 * sh.shCoeffs[2].xyz * N.z +
        1.023328 * sh.shCoeffs[3].xyz * N.x +
        0.858086 * sh.shCoeffs[4].xyz * N.x * N.y +
        0.858086 * sh.shCoeffs[5].xyz * N.y * N.z +
        0.247708 * sh.shCoeffs[6].xyz * (3.0 * N.z * N.z - 1.0) +
        0.858086 * sh.shCoeffs[7].xyz * N.x * N.z +
        0.429043 * sh.shCoeffs[8].xyz * (N.x * N.x - N.y * N.y)
    );
}

float traceVisibility(vec3 origin, vec3 dir, float tMax) {
    rayQueryEXT shadowQuery;
    rayQueryInitializeEXT(shadowQuery, topLevelAS, gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, 0.001, dir, tMax);
    while (rayQueryProceedEXT(shadowQuery)) {}

    return rayQueryGetIntersectionTypeEXT(shadowQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT ? 1.0 : 0.0;
}

// LightComponent lights with the light pass falloff (intensity / d^2), each one shadow tested.
// origin should already be pushed off the surface along N
vec3 localLightIrradiance(vec3 origin, vec3 N, uint64_t lightsRef, uint lightCount) {
    vec3 irradiance = vec3(0.0);
    if (lightCount == 0u) {
        return irradiance;
    }

    LightsBuffer lights = LightsBuffer(lightsRef);
    for (uint i = 0u; i < lightCount; i++) {
        PointLight light = lights.lights[i];
        vec3 toLight = light.position.xyz - origin;
        float distance2 = max(dot(toLight, toLight), 1e-4);
        float distance = sqrt(distance2);
        vec3 L = toLight / distance;

        float NdotL = dot(N, L);
        if (NdotL <= 0.0) {
            continue;
        }

        vec3 contribution = light.color.rgb * light.intensity / distance2 * NdotL;
        // skip the shadow ray for lights too far or too dim to matter
        if (dot(contribution, vec3(0.2126, 0.7152, 0.0722)) < 1e-3) {
            continue;
        }

        irradiance += contribution * traceVisibility(origin, L, distance - 0.01);
    }
    return irradiance;
}

#endif
