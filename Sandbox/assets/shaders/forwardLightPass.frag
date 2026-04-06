#version 460

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

layout(set = 0, binding = 0)  uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
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

layout(set = 1, binding = 0) uniform sampler2D albedoMaps;
layout(set = 1, binding = 1) uniform sampler2D normalMaps;
layout(set = 1, binding = 2) uniform sampler2D metalnessMaps;
layout(set = 1, binding = 3) uniform sampler2D roughnessMaps;
layout(set = 1, binding = 4) uniform sampler2D aoMaps;
layout(set = 1, binding = 5) uniform sampler2D emissiveMaps;

layout(push_constant) uniform PushConstantLight {
    mat4 sunlightMVP;
    vec4 direction;
    vec4 color;
    float bias;
    float alpha;
    float lintstepLow;
    float linstepHigh;
    float litBias;
    float time;
    float numLights;
    float skyboxDetail;
} pcl;

const float PI = 3.14159265359;

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

float calcPCSS(vec3 worldPos, vec3 worldNorm) {
    vec3 L = normalize(pcl.direction.xyz);
    
    // UVs are nudged to sample clean areas of the shadow map
    vec3 offsetWorldPos = worldPos + worldNorm * pcl.bias;
    vec4 fragPosLightSpace = pcl.sunlightMVP * vec4(offsetWorldPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    
    // true depth for contact hardening
    vec4 fragPosTrue = pcl.sunlightMVP * vec4(worldPos, 1.0);
    float currentDepth = fragPosTrue.z / fragPosTrue.w;

    if(projCoords.z > 1.0) {
        return 0.0;
    }

    float avgBlockerDepth = 0.0;
    int blockers = 0;
    const float LIGHT_SIZE_UV = 0.05; 
    float searchRegion = LIGHT_SIZE_UV * (currentDepth); 

    float searchBias = max(0.002 * (1.0 - dot(worldNorm, L)), 0.0005);

    for(int i = -2; i <= 2; ++i) {
        for(int j = -2; j <= 2; ++j) {
            vec2 offset = vec2(i, j) * (searchRegion / 5.0);
            float depth = texture(shadowMap, projCoords.xy + offset).r;
            
            if(depth < currentDepth - searchBias) { 
                avgBlockerDepth += depth;
                blockers++;
            }
        }
    }

    if(blockers == 0) {
        return 0.0;
    }
    avgBlockerDepth /= float(blockers);

    // float penumbra = ((currentDepth - avgBlockerDepth) * LIGHT_SIZE_UV) / avgBlockerDepth;
    float penumbra = (currentDepth - avgBlockerDepth) * LIGHT_SIZE_UV;
    penumbra = clamp(penumbra, 0.0, 0.02); 

    vec2 noiseUV = gl_FragCoord.xy / vec2(textureSize(blueNoise, 0));
    float noiseValue = texture(blueNoise, noiseUV).r;

    float angle = noiseValue * 6.283185;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rotation = mat2(c, -s, s, c);

    const vec2 poissonDisk32[16] = vec2[](
        vec2( -0.94201624, -0.39906216 ), vec2( 0.94558609, -0.76890725 ), 
        vec2( -0.094184101, -0.92938870 ), vec2( 0.34495938, 0.29387760 ), 
        vec2( -0.91588581, 0.45771432 ), vec2( -0.81544232, -0.87912464 ), 
        vec2( -0.38277543, 0.27676845 ), vec2( 0.97484398, 0.75648379 ), 
        vec2( 0.44323325, -0.97511554 ), vec2( 0.53742981, -0.47373420 ), 
        vec2( -0.65433973, 0.025204695 ), vec2( -0.43765828, -0.46990421 ), 
        vec2( 0.35489357, -0.27411318 ), vec2( -0.21171454, -0.11072331 ), 
        vec2( 0.73039985, -0.23011690 ), vec2( 0.51726257, 0.43840042 )
    );

    float pcfBias = max(0.001 * (1.0 - dot(worldNorm, L)), 0.0001);

    float shadow = 0.0;
    for (int i = 0; i < 16; i++) {
        vec2 offset = (rotation * poissonDisk32[i]) * penumbra;
        float pcfDepth = texture(shadowMap, projCoords.xy + offset).r;
        
        if (currentDepth - pcfBias > pcfDepth) {
            shadow += 1.0;
        }
    }

    return shadow / 16.0;
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
    vec3 albedo     = texture(albedoMaps, fragTexCoord).rgb;
    float ao        = texture(aoMaps, fragTexCoord).r;
    float roughness = texture(roughnessMaps, fragTexCoord).g;
    float metallic  = texture(metalnessMaps, fragTexCoord).b;
    
    // float alphaRoughness = roughness * roughness;
    // alphaRoughness = clamp(alphaRoughness, 0.05, 1.0); 

    vec3 tangentNormal = texture(normalMaps, fragTexCoord).rgb * 2.0 - 1.0;
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

    for(int i = 0; i < int(pcl.numLights); i++) {
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

    vec3 L = normalize(pcl.direction.xyz);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    
    float shadow = 1.0 - calcPCSS(fragWorldPos, N);
    
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(N, H, roughness);       
    float G   = GeometrySmith(N, V, L, roughness);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator; 

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    
    Lo += (kD * albedo / PI + specular) * pcl.color.rgb * NdotL * shadow;

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

    vec3 emissive = texture(emissiveMaps, fragTexCoord).rgb;
    vec3 color = ambient + Lo + emissive;

    vec3 V_dir = normalize(fragWorldPos - ubo.cameraPos.xyz);
    float maxDist = length(fragWorldPos - ubo.cameraPos.xyz);


    // outColor = vec4(textureLod(prefilterMap, R, 1.0 * MAX_LOD).rgb, 1.0);
    // return;
    // outColor = vec4(F_ibl, 1.0);
    // return;
    // outColor = vec4(ambient, 1.0);
    // return;
    // outColor = vec4(kD_ibl, 1.0);
    // return;
    // outColor = vec4(specularAmbient, 1.0);
    // return;
    // outColor = vec4(vec3(1.0 - roughness), 1.0); 
    // return;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    if (any(isnan(color))) {
        color = vec3(0.0);
    }
    outColor = vec4(color, 1.0);
}