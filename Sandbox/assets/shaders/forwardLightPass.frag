#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "common/buffers.glsl"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

layout(location = 0) out vec4 outColor;

struct Light {
    vec4 color;      
    vec4 position;
    float intensity;
};

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
    mat4 prevViewProj;
    mat4 proj;
    vec4 cameraPos;
    mat4 invView;
    mat4 invProj;
    float width;
    float height;
} ubo;

layout(set = 0, binding = 2, std430) readonly buffer LightSSBO {
    Light lights[];
} lightSSBO;

layout(set = 0, binding = 3) uniform sampler2D shadowMap;
layout(set = 0, binding = 4) uniform sampler2D blueNoise;

layout(set = 0, binding = 5) readonly buffer SHData {
    vec4 shCoeffs[9];
} sh;

layout(set = 0, binding = 6) uniform sampler2D brdfLUT;
layout(set = 0, binding = 7) uniform samplerCube prefilterMap;
layout(set = 0, binding = 8) uniform sampler2D hdrImage;

layout(set = 1, binding = 0) uniform sampler2D samplerImages[];
// layout(set = 2, binding = 1) uniform image2D storageImages[];
// layout(set = 2, binding = 2) uniform sampler storageImages[];
layout(buffer_reference, scalar) buffer BoneModelBuffer { mat4 boneMatrices[]; };
layout(buffer_reference, scalar) buffer BoneMatrixArrayTable { BoneModelBuffer ptrs[]; };

layout(buffer_reference, scalar) buffer Materials{ Material m[]; };

layout(push_constant) uniform PushConstant {
    Materials materialsRef;
    float bias;
    float time;
    mat4 sunlightMVP;
    vec4 direction;
    // vec4 color;
    float numLights;
    uint materialIdx;
    uint boneMatricesIdx;
    BoneMatrixArrayTable boneTableRef;
} pc;


float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// how fast the shadow edge widens with distance from the occluder (tan of the light's angular radius, exaggerated)
const float SUN_SOFTNESS = 0.04;
// widest penumbra in world units, also how far the blocker search looks
const float MAX_PENUMBRA = 0.75;

// compares against the 4 nearest texels and filters the results.
// sampling the depth with a linear sampler and comparing afterwards filters the wrong thing and gives stair steps
float shadowTap(vec2 uv, float compareDepth) {
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return 1.0;     // outside the shadow map: lit
    }
    vec2 f = fract(uv * vec2(textureSize(shadowMap, 0)) - 0.5);
    vec4 lit = step(compareDepth, textureGather(shadowMap, uv, 0));     // texels (0,1) (1,1) (1,0) (0,0)
    return mix(mix(lit.w, lit.z, f.x), mix(lit.x, lit.y, f.x), f.y);
}

// evenly spread points on a disk (golden angle spiral). rotated per pixel it covers the disk far more evenly than
// a fixed poisson set, which is what keeps the penumbra from looking grainy
vec2 vogelDisk(int i, int count, float phi) {
    float r = sqrt((float(i) + 0.5) / float(count));
    float theta = float(i) * 2.39996323 + phi;
    return r * vec2(cos(theta), sin(theta));
}

const int BLOCKER_TAPS = 16;
const int FILTER_TAPS = 32;

// returns how much of the sun is blocked, 0 = lit, 1 = fully in shadow
float calcPCSS(vec3 worldPos, vec3 worldNorm, mat4 lightMVP, vec3 L) {
    // the light is orthographic, so world units convert to shadow map units with two constants
    // read straight from the matrix (they follow the sun view area / zNear / zFar sliders)
    float uvPerUnit = 0.5 * length(vec3(lightMVP[0][0], lightMVP[1][0], lightMVP[2][0]));
    float depthPerUnit = length(vec3(lightMVP[0][2], lightMVP[1][2], lightMVP[2][2]));

    float NdotL = dot(worldNorm, L);
    float normalOffset = max(0.05 * (1.0 - NdotL), 0.005);
    vec4 fragPosLightSpace = lightMVP * vec4(worldPos + worldNorm * normalOffset, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    if (projCoords.z > 1.0) {
        return 0.0;
    }

    float texel = 1.0 / float(textureSize(shadowMap, 0).x);
    float depthBias = 0.05 * depthPerUnit;
    float receiverDepth = projCoords.z - depthBias;

    vec2 noiseUV = gl_FragCoord.xy / vec2(textureSize(blueNoise, 0));
    float angle = texture(blueNoise, noiseUV).r * 2.0 * PI;

    // blocker search: has to cover the widest penumbra, or the soft edge gets cut off where the search stops finding blockers
    float searchRadius = MAX_PENUMBRA * uvPerUnit;
    float blockerDepth = 0.0;
    float blockers = 0.0;
    for (int i = 0; i < BLOCKER_TAPS; i++) {
        vec2 uv = projCoords.xy + vogelDisk(i, BLOCKER_TAPS, angle) * searchRadius;
        float depth = textureLod(shadowMap, uv, 0.0).r;
        if (depth < receiverDepth) {
            blockerDepth += depth;
            blockers += 1.0;
        }
    }
    if (blockers < 1.0) {
        return 0.0;
    }
    blockerDepth /= blockers;

    // contact hardening: the further the occluder is from the receiver, the wider the edge.
    // for an orthographic light that is just distance * softness, there is no divide by the blocker depth
    float occluderDistance = (receiverDepth - blockerDepth) / depthPerUnit;
    float penumbra = clamp(occluderDistance * SUN_SOFTNESS, 0.0, MAX_PENUMBRA) * uvPerUnit;
    penumbra = max(penumbra, texel * 1.5);

    float lit = 0.0;
    for (int i = 0; i < FILTER_TAPS; i++) {
        lit += shadowTap(projCoords.xy + vogelDisk(i, FILTER_TAPS, angle) * penumbra, receiverDepth);
    }

    return 1.0 - lit / float(FILTER_TAPS);
}
const mat4 DITHER_PATTERN = mat4(
    vec4(0.0, 0.5, 0.125, 0.625),
    vec4(0.75, 0.22, 0.875, 0.375),
    vec4(0.1875, 0.6875, 0.0625, 0.5625),
    vec4(0.9375, 0.4375, 0.8125, 0.3125)
);

float mieScattering(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

float rand(vec3 p) {
    return fract(sin(dot(p, vec3(12.345, 67.89, 412.12))) * 42123.45) * 2.0 - 1.0;
}

// single octave noise
float simpleNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(rand(i), rand(i + vec3(1, 0, 0)), f.x),
               mix(rand(i + vec3(0, 1, 0)), rand(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(rand(i + vec3(0, 0, 1)), rand(i + vec3(1, 0, 1)), f.x),
               mix(rand(i + vec3(0, 1, 1)), rand(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

void main() {
    Material mat = pc.materialsRef.m[pc.materialIdx];
    
    vec2 frameUV = (fragTexCoord * mat.uvScale) + mat.uvOffset;
    if (any(lessThan(mat.uvScale, vec2(1.0 - 1e-4)))) { //treats binlinear filtering
        vec2 texSize = vec2(textureSize(samplerImages[mat.albedoIdx], 0));
        vec2 halfTexel = 0.5 / texSize;

        vec2 cellMin = mat.uvOffset + halfTexel;
        vec2 cellMax = mat.uvOffset + mat.uvScale - halfTexel;
        frameUV = clamp(frameUV, cellMin, cellMax);
    }

    vec3 albedo     = texture(samplerImages[mat.albedoIdx], frameUV).rgb;
    float ao        = texture(samplerImages[mat.aoIdx], fragTexCoord).r;
    float roughness = texture(samplerImages[mat.roughnessIdx], fragTexCoord).g;
    float metallic  = texture(samplerImages[mat.metalnessIdx], fragTexCoord).b;

    if (texture(samplerImages[mat.albedoIdx], frameUV).a < 0.01) { 
        discard;
    }
    
    albedo *= mat.albedoFactor.rgb;
    ao *= mat.aoFactor;
    roughness *= mat.roughnessFactor;
    metallic *= mat.metallicFactor;
    
    // float alphaRoughness = roughness * roughness;
    // alphaRoughness = clamp(alphaRoughness, 0.05, 1.0); 

    // vec3 tangentNormal = texture(normalMaps, fragTexCoord).rgb * 2.0 - 1.0;
    vec3 tangentNormal = texture(samplerImages[mat.normalIdx], fragTexCoord).rgb * 2.0 - 1.0;
    mat3 TBN = mat3(normalize(inTangent), normalize(inBitangent), normalize(inNormal));
    vec3 V = normalize(ubo.cameraPos.xyz - fragWorldPos);
    vec3 geomN = normalize(inNormal);

    float side = dot(geomN, V) >= 0.0 ? 1.0 : -1.0; // determine the side based on the raw triangle geometry
    vec3 faceNormal = geomN * side;

    vec3 N = normalize(TBN * tangentNormal) * side;

    // orizon correction prevents the normal map from pointing away from the camera 
    // relative to the triangle surface.
    // If the normal map pushes the vector behind the camera, mix it back toward the face normal until it's safe.
    float NdotV_unclamped = dot(N, V);
    if (NdotV_unclamped < 0.0) {
        N = normalize(N - V * NdotV_unclamped);
    }

    N = normalize(mix(N, V, 0.01)); 

    // float NdotV = clamp(dot(N, V), 0.001, 1.0);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    for(int i = 0; i < int(pc.numLights); i++) {
        vec3 L = normalize(lightSSBO.lights[i].position.xyz - fragWorldPos);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0001);
        
        float dist = length(lightSSBO.lights[i].position.xyz - fragWorldPos);
        float attenuation = lightSSBO.lights[i].intensity / (dist * dist + 1.0);
        vec3 radiance = lightSSBO.lights[i].color.rgb * attenuation;

        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
        
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 L = normalize(pc.direction.xyz);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    
    float shadow = 1.0 - calcPCSS(fragWorldPos, N, pc.sunlightMVP, normalize(pc.direction.xyz));
    
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(N, H, roughness);       
    float G   = GeometrySmith(N, V, L, roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator; 

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    
    // Lo += (kD * albedo / PI + specular) * pc.color.rgb * NdotL * shadow;
    vec3 tempColor = vec3(1.0);
    Lo += (kD * albedo / PI + specular) * tempColor * NdotL * shadow;

    vec3 R = reflect(-V, N);
    vec3 F_ibl = fresnelSchlickRoughness(NdotV, F0, roughness);
    
    vec3 irradiance = sh.shCoeffs[0].rgb;
    if(length(irradiance) < 0.001) {
        irradiance = vec3(0.03);
    }
    vec3 diffuseAmbient = irradiance * albedo;

    // vec2 envBRDF = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec2 envBRDF = texture(brdfLUT, vec2(NdotV, 1.0 - roughness)).rg;
    const float MAX_LOD = 7.0; 
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_LOD).rgb;
    vec3 specularAmbient = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);

    vec3 kD_ibl = (1.0 - F_ibl) * (1.0 - metallic);
    vec3 ambient = (kD_ibl * diffuseAmbient + specularAmbient) * ao;
    // float specularOcclusion = clamp(pow(NdotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
    // vec3 ambient = (kD_ibl * diffuseAmbient * ao) + (specularAmbient * specularOcclusion);

    vec3 emissive = texture(samplerImages[mat.emissiveIdx], fragTexCoord).rgb * mat.emissiveFactor;
    vec3 color = ambient + Lo + emissive;

    vec3 V_dir = normalize(fragWorldPos - ubo.cameraPos.xyz);
    float maxDist = length(fragWorldPos - ubo.cameraPos.xyz);
    
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    if (any(isnan(color))) {
        color = vec3(0.0);
    }
    outColor = vec4(color, 1.0);
}