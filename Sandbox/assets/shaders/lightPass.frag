#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inputNorm;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inputAlbedo;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput inputPBR;

layout(location = 0) out vec4 outColor;

struct Light {
    vec4 color;      
    vec4 position;
    float intensity;
};

layout(set = 1, binding = 0)  uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(set = 1, binding = 1, std430) readonly buffer LightSSBO {
    Light lights[];
} lightSSBO;

layout(set = 1, binding = 2) uniform sampler2D shadowMap;
layout(set = 1, binding = 3) uniform sampler2D blueNoise;

layout(set = 1, binding = 4) readonly buffer SHData {
    vec4 shCoeffs[9];
} sh;

layout (push_constant) uniform LightData {
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
} pcl;

vec3 getIrradiance(vec3 N) {
    // These constants already include the 1/PI and Lambertian factors
    return max(vec3(0.0),
        0.886227 * sh.shCoeffs[0].xyz +                            // L00
        1.023328 * sh.shCoeffs[1].xyz * N.y +                      // L1-1
        1.023328 * sh.shCoeffs[2].xyz * N.z +                      // L10
        1.023328 * sh.shCoeffs[3].xyz * N.x +                      // L11
        0.858086 * sh.shCoeffs[4].xyz * N.x * N.y +                // L2-2
        0.858086 * sh.shCoeffs[5].xyz * N.y * N.z +                // L2-1
        0.247708 * sh.shCoeffs[6].xyz * (3.0 * N.z * N.z - 1.0) +  // L20
        0.858086 * sh.shCoeffs[7].xyz * N.x * N.z +                // L21
        0.429043 * sh.shCoeffs[8].xyz * (N.x * N.x - N.y * N.y)    // L22
    );
}

float calcShadow(vec3 worldPos) {
    vec4 fragPosLightSpace = pcl.sunlightMVP * vec4(worldPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if(projCoords.z > 1.0){
        return 0.0;
    }

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    for(int x = -1; x <= 1; ++x) {  // 3x3 PCF Kernel
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += (projCoords.z - 0.002) > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    
    return shadow / 9.0;
}


float linstep(float low, float high, float v) {
    return clamp((v - low) / (high - low), 0.0, 1.0);
}


float calcMSMShadow(vec3 worldPos) {
    vec4 fragPosLightSpace = pcl.sunlightMVP * vec4(worldPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if(projCoords.z > 1.0 || projCoords.z < 0.0) return 0.0;

    vec4 b = texture(shadowMap, projCoords.xy);
    
    vec4 m = (1.0 - pcl.alpha) * b + pcl.alpha * vec4(0.5, 0.5, 0.5, 0.5);

    float d = projCoords.z;
    // float d = projCoords.z * 2.0 - 1.0;
    
    
    if (d <= m.x + pcl.litBias) { //self shadow bias
        return 0.0;
    }

    float L21 = m.y - m.x * m.x;
    float L31 = m.z - m.y * m.x;
    float L32 = m.w - m.y * m.y - (L31 * L31) / L21;
    
    float f1 = d - m.x;
    float f2 = d * d - m.y;
    
    float c1 = f1 / L21;
    float c2 = (f2 - L31 * c1) / L32;
    
    float p = (c2 * f2 + c1 * f1);
    float shadow = p / (1.0 + p);

    shadow = linstep(pcl.lintstepLow, pcl.linstepHigh, shadow);
    // shadow = pow(shadow, 2.0); 
    // shadow = clamp(shadow * 1.5 - 0.1, 0.0, 1.0);

    return shadow;
}

float random(vec3 seed) {
    return fract(sin(dot(seed, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
}

float calcPCSS(vec3 worldPos) {
    vec4 fragPosLightSpace = pcl.sunlightMVP * vec4(worldPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    if(projCoords.z > 1.0) return 0.0;

    float avgBlockerDepth = 0.0;
    int blockers = 0;
    
    // Increase this to 0.1 or 0.2 to force a massive, obvious blur for testing.
    const float LIGHT_SIZE_UV = 0.05; 
    
    // wide search to find blockers that are far away
    float searchRegion = LIGHT_SIZE_UV * (currentDepth); 

    // small grid for blocker search
    for(int i = -2; i <= 2; ++i) {
        for(int j = -2; j <= 2; ++j) {
            vec2 offset = vec2(i, j) * (searchRegion / 5.0);
            vec3 L = normalize(pcl.direction.xyz);
            vec3 worldNorm = normalize(subpassLoad(inputNorm).rgb);
            float bias = max(0.005 * (1.0 - dot(worldNorm, L)), 0.0005);
            float depth = texture(shadowMap, projCoords.xy + offset).r;
            if(depth < currentDepth - bias) { 
                avgBlockerDepth += depth;
                blockers++;
            }
        }
    }

    if(blockers == 0) {
        return 0.0; 
    }
    avgBlockerDepth /= float(blockers);

    // Receiver - Blocker) / Blocker
    // float penumbra = (currentDepth - avgBlockerDepth) * LIGHT_SIZE_UV / avgBlockerDepth;
    float penumbra = (currentDepth - avgBlockerDepth) * LIGHT_SIZE_UV;
    
    penumbra = clamp(penumbra, 0.0, 0.02); 

    vec2 noiseUV = gl_FragCoord.xy / vec2(textureSize(blueNoise, 0));
    // noiseUV += vec2(pcl.time * 0.1337, pcl.time * 0.4337);
    float noiseValue = texture(blueNoise, noiseUV).r;
    // float noiseValue = random(worldPos + vec3(pcl.time));

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

    float shadow = 0.0;
    for (int i = 0; i < 16; i++) {
        vec2 offset = (rotation * poissonDisk32[i]) * penumbra;
        float pcfDepth = texture(shadowMap, projCoords.xy + offset).r;
        
        if (currentDepth - 0.0015 > pcfDepth) {
            shadow += 1.0;
        }
    }
    return shadow / 16.0;
}

const float PI = 3.14159265359;

// approximates the amount of microfacets aligned with the halfway vector (specular glints)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// self-shadowing approximation of the microfacets
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
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

vec3 calcPBR(
    vec3 L, vec3 V, vec3 N, vec3 F0, 
    vec3 albedo, float roughness, float metallic, vec3 radiance
) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    
    float D = DistributionGGX(N, H, roughness);   
    float G = GeometrySmith(N, V, L, roughness);      
    vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = D * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    vec3 worldPos  = subpassLoad(inputPos).rgb;
    vec3 worldNorm = normalize(subpassLoad(inputNorm).rgb);
    vec4 albedo    = subpassLoad(inputAlbedo);
    vec3 pbr       = subpassLoad(inputPBR).rgb; 

    if(length(worldNorm) < 0.1) {
        discard;
    }

    float ao        = pbr.r;
    float roughness = pbr.g;
    float metallic  = pbr.b;
    
    vec3 N = worldNorm;
    vec3 V = normalize(ubo.cameraPos.xyz - worldPos);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo.rgb, metallic);
    
    vec3 Lo = vec3(0.0);

    for(int i = 0; i < pcl.numLights ; i++) {
        vec3 lightPos   = lightSSBO.lights[i].position.xyz;
        vec3 lightCol   = lightSSBO.lights[i].color.rgb;
        float intensity = lightSSBO.lights[i].intensity;

        vec3 L = normalize(lightPos - worldPos);

        float distance    = length(lightPos - worldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = lightCol * attenuation;
        radiance = radiance * intensity;   // light control with intensity bound to uniform scale 

        vec3 contribution = calcPBR(L, V, N, F0, albedo.rgb, roughness, metallic, radiance);
        Lo += contribution;
    }
    
    // float shadow = calcShadow(worldPos);
    // float shadow = calcMSMShadow(worldPos + worldNorm * pcl.bias);
    float shadow = calcPCSS(worldPos);
    vec3 L_sun = normalize(pcl.direction.xyz); 
    vec3 sunRadiance = pcl.color.rgb * (1 - shadow);
    vec3 sunlight = calcPBR(L_sun, V, N, F0, albedo.rgb, roughness, metallic, sunRadiance);


    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;	  
    
    vec3 irradiance = getIrradiance(N); 
    vec3 ambient = (kD * irradiance * albedo.rgb) * ao;
    // vec3 ambient = vec3(0.03) * albedo.rgb * ao;
    vec3 finalColor = ambient + Lo + sunlight;

    finalColor = finalColor / (finalColor + vec3(1.0));     //HDR tone mapping
    outColor = vec4(pow(finalColor, vec3(1.0/2.2)), 1.0);   //Gamma correction
}
