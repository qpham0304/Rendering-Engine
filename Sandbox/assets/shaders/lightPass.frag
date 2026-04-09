#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inputNorm;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inputAlbedo;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput inputPBR;
layout(input_attachment_index = 4, set = 0, binding = 4) uniform subpassInput inputEmissive;

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
    mat4 invView;
    mat4 invProj;
    float width;
    float height;
} ubo;

layout(set = 1, binding = 1, std430) readonly buffer LightSSBO {
    Light lights[];
} lightSSBO;

layout(set = 1, binding = 2) uniform sampler2D shadowMap;
layout(set = 1, binding = 3) uniform sampler2D blueNoise;

layout(set = 1, binding = 4) readonly buffer SHData {
    vec4 shCoeffs[9];
} sh;

layout(set = 1, binding = 5) uniform sampler2D brdfLUT;
layout(set = 1, binding = 6) uniform samplerCube prefilterMap;
layout(set = 1, binding = 7) uniform sampler2D hdrImage;
layout(set = 1, binding = 8) uniform sampler2D aoImage;

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
    float skyboxDetail;
    int aoOn;
    float G;
    float scatteringScale;
} pcl;

const float PI = 3.14159265359;
const vec2 invAtan = vec2(0.1591, 0.3183); // 1.0/(2.0*PI), 1.0/PI

vec2 sampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}


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

const vec2 poissonDisk16[16] = vec2[](
    vec2(-0.1741, 0.0197), vec2(0.0263, -0.1667),
    vec2(0.1556, 0.1533), vec2(-0.2520, -0.3392),
    vec2(0.4209, -0.0654), vec2(-0.1610, 0.4448),
    vec2(-0.4557, 0.1623), vec2(0.2057, -0.5641),
    vec2(0.4706, 0.4578), vec2(-0.6861, -0.2227),
    vec2(0.3204, 0.6955), vec2(0.6874, -0.4045),
    vec2(-0.3541, -0.7423), vec2(-0.8354, 0.3582),
    vec2(0.7936, 0.3807), vec2(-0.1172, 0.9525)
);


float calcPCSS(vec3 worldPos) {
    vec3 worldNorm = normalize(subpassLoad(inputNorm).rgb);
    vec3 L = normalize(pcl.direction.xyz);
    float NdotL = dot(worldNorm, L);

    float biasScale = max(0.05 * (1.0 - NdotL), 0.005); 
    vec4 fragPosLightSpace = pcl.sunlightMVP * vec4(worldPos + worldNorm * biasScale, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    vec2 noiseUV = gl_FragCoord.xy / vec2(textureSize(blueNoise, 0));
    float noiseValue = texture(blueNoise, noiseUV).r;

    const float LIGHT_SIZE_UV = 0.005; 
    float avgBlockerDepth = 0.0;
    int blockers = 0;
    float searchRegion = LIGHT_SIZE_UV * (projCoords.z); 

    for(int i = -2; i <= 2; ++i) {
        for(int j = -2; j <= 2; ++j) {
            vec2 offset = (vec2(i, j) + (noiseValue - 0.5)) * (searchRegion / 5.0);
            float depth = texture(shadowMap, projCoords.xy + offset).r;
            float distanceToBlocker = projCoords.z - depth;
            if(depth < projCoords.z - 0.0005 && distanceToBlocker > 0.001) { 
                avgBlockerDepth += depth;
                blockers++;
            }
        }
    }

    if(blockers < 1) {
        return 0.0;
    }
    avgBlockerDepth /= float(blockers);

    // contact hardening penumbra
    float penumbra = (projCoords.z - avgBlockerDepth) * LIGHT_SIZE_UV / avgBlockerDepth;
    penumbra = clamp(penumbra, 0.0, 0.01);

    float texelSize = 1.25 / 1024.0;    // fall back blur for low detail shadow
    penumbra = max(penumbra, texelSize * 1.25);

    float shadow = 0.0;
    float angle = noiseValue * 2 * PI;
    mat2 rotation = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));

    for (int i = 0; i < 16; i++) {
        // rotate and jitter the disk to eliminate banding
        vec2 offset = (rotation * poissonDisk16[i]) * penumbra;
        float pcfDepth = texture(shadowMap, projCoords.xy + offset).r;
        
        if (projCoords.z - 0.0005 > pcfDepth) {
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

// Henyey-Greenstein phase function
float mieScattering(float cosTheta, float G) {
	float numerator = (1.0f - G*G);
	float denominator =  (4.0f * PI * pow(1.0f + G * G - 2.0f * G * cosTheta, 1.5f));
	return numerator / denominator;
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

float fbm(vec3 p) 
{
    vec3 q = p;
    int numOctaves = 8;
    float weight = 0.5;
    float ret = 0.0;
    
    for (int i = 0; i < numOctaves; i++)
    {
        ret += weight * simpleNoise(q); 
        q *= 2.0;
        weight *= 0.5;
    }
    return clamp(ret, 0.0, 1.0);
}

float linearizeDepth(float depth) {
	float near = 0.1f;
	float far = 100.0f;
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

float logDepth(float depth, float steepness, float offset) {
	float zVal = linearizeDepth(depth);
	return (1 / (1 + exp(-steepness * (zVal - offset))));
}


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
    float NdotL = max(dot(N, L), 0.0001);
    float NdotV = max(dot(N, V), 0.0001);

    float D = DistributionGGX(N, H, roughness);   
    float G = GeometrySmith(N, V, L, roughness);      
    vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = D * G * F; 
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

// generates a sample vector biased towards the specular lobe (GGX)
vec3 ImportanceSampleGGX(vec2 xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a*a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    // grom tangent-space vector to world-space sample vector
    vec3 up          = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent     = normalize(cross(up, N));
    vec3 bitangent   = cross(N, tangent);
    
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// importance sampling specular function
vec3 GetImportanceSampledSpecular(vec3 V, vec3 N, float roughness, vec3 F0) {
    vec3 specularLighting = vec3(0.0);
    const uint SAMPLE_COUNT = 32u; // Increase for quality, decrease for speed
    
    for(uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(xi, N, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0) {
            vec3 lookupL = vec3(L.x, L.y, -L.z);
            vec2 uv = sampleSphericalMap(lookupL);
            vec3 sampleColor = texture(hdrImage, uv).rgb;

            specularLighting += sampleColor * NdotL;
        }
    }
    
    return specularLighting / float(SAMPLE_COUNT);
}

vec3 aces(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 worldPos  = subpassLoad(inputPos).rgb;
    vec3 worldNorm = subpassLoad(inputNorm).rgb;
    vec4 albedo    = subpassLoad(inputAlbedo);
    vec3 pbr       = subpassLoad(inputPBR).rgb;
    vec4 emissive  = subpassLoad(inputEmissive);

    if(length(worldNorm) < 0.1) {
        vec2 texCoord = gl_FragCoord.xy / vec2(ubo.width, ubo.height); 
        vec4 clipPos = vec4(texCoord * 2.0 - 1.0, 1.0, 1.0);

        vec4 viewPos = ubo.invProj * clipPos;
        viewPos /= viewPos.w;

        vec3 V_sky = mat3(ubo.invView) * normalize(viewPos.xyz);
        vec3 V_sky_norm = normalize(V_sky);
        vec3 lookupV = vec3(V_sky_norm.x, V_sky_norm.y, -V_sky_norm.z);

        vec2 uv = sampleSphericalMap(lookupV);
        vec3 hdrColor = texture(hdrImage, uv).rgb;
        
        float detail = pcl.skyboxDetail;
        float maxLod = 7.0;
        vec3 prefilteredColor = textureLod(prefilterMap, lookupV, detail * maxLod).rgb;
        
        vec3 irradiance = getIrradiance(V_sky_norm);

        vec3 envColor;
        // if(detail < 0.5) {
        //     envColor = mix(hdrColor, prefilteredColor, detail * 2.0);
        // } else {
            float blendWeight = clamp(detail * 2.0 - 1.0, 0.0, 1.0);
            envColor = mix(prefilteredColor, irradiance, blendWeight);
        // }

        // envColor = envColor / (envColor + 1.0);
        // outColor = vec4(pow(envColor, vec3(1.0/2.2)), 1.0);
        outColor = vec4(envColor, 1.0); 
        return;
    }
    
    float ao        = pbr.r;
    float roughness = pbr.g;
    float metallic  = pbr.b;
    
    vec3 V = normalize(ubo.cameraPos.xyz - worldPos);
    vec3 N = normalize(worldNorm);
    if (dot(N, V) < 0.0) {
        N = -N;
    }

    N = normalize(mix(N, V, 0.015)); 

    float NdotV = clamp(dot(N, V), 0.001, 1.0);
    // float NdotV = max(dot(N, V), 0.0001);

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
    
    // float shadow = 1.0 - calcShadow(worldPos);
    // float shadow = 1.0 - calcMSMShadow(worldPos + N * pcl.bias);
    float shadow = 1.0 - calcPCSS(worldPos);
    vec3 L_sun = normalize(pcl.direction.xyz); 
    vec3 sunRadiance = pcl.color.rgb * shadow;
    vec3 sunlight = calcPBR(L_sun, V, N, F0, albedo.rgb, roughness, metallic, sunRadiance);

    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;	  
    
    vec3 irradiance = getIrradiance(N);
    vec3 diffuseIBL = irradiance * albedo.rgb;

    const float MAX_REFLECTION_LOD = 4.0; 
    vec3 R = reflect(-V, N); 
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
    // vec3 specularIBL = GetImportanceSampledSpecular(V, N, roughness, F0) * F;
    
    vec2 screenUV = gl_FragCoord.xy / vec2(ubo.width, ubo.height);
    float ssao = texture(aoImage, screenUV).r;
    
    if(pcl.aoOn != 0) {
        ao *= ssao;
    }
    
    vec3 ambient = (kD * diffuseIBL + specularIBL) * ao;
    vec3 finalColor = ambient + Lo + sunlight + emissive.rgb;

    // if(pcl.aoOn != 0) {
    //     outColor = outColor = vec4(vec3(ssao * pbr.r), 1.0);
    //     return;
    // }

    // vec3 V_dir = normalize(worldPos - ubo.cameraPos.xyz);
    vec3 V_ray = normalize(worldPos - ubo.cameraPos.xyz);
    float maxDist = length(worldPos - ubo.cameraPos.xyz);

    const int numSteps = 16;
    float stepSize = maxDist / float(numSteps);

    // Dithering to hide banding
    float dither = DITHER_PATTERN[int(gl_FragCoord.x) % 4][int(gl_FragCoord.y) % 4];
    vec2 noiseUV = gl_FragCoord.xy / vec2(textureSize(blueNoise, 0));
    dither *= texture(blueNoise, noiseUV).r;
    // vec3 rayPos = ubo.cameraPos.xyz + (V_ray * stepSize * dither);
    vec3 rayPos = ubo.cameraPos.xyz;
    vec3 step = V_ray * stepSize;
    rayPos += step * dither;

    vec3 volume = vec3(0.0f);
	vec4 color = vec4(0.0f);
    float transparency = 1.0;
    float absorptionCoefficient = 0.15;
    for(int i = 0; i < numSteps; i++) {
        vec4 fragPosLight = pcl.sunlightMVP * vec4(rayPos, 1.0);
        vec3 projCoords = fragPosLight.xyz / fragPosLight.w;
        projCoords.xy = projCoords.xy * 0.5 + 0.5;

        float shadowDepth = texture(shadowMap, projCoords.xy).r;

        // is the air-point closer to sun than the shadow-map
        if(shadowDepth > projCoords.z - 0.0005) {
            float phase = mieScattering(dot(V_ray, L_sun), pcl.G);
            vec3 scattering = pcl.color.rgb * phase * absorptionCoefficient;
            volume += scattering * transparency * stepSize;
        }
        transparency *= exp(-absorptionCoefficient * stepSize);
        
        if(transparency <= 0.01) {
            break;
        }
        rayPos += step;
    }

    vec3 finalVolume = (volume / float(numSteps)) * (pcl.scatteringScale*3);
    finalColor += finalVolume;
    finalColor = finalColor / (finalColor + vec3(1.0));
    
    outColor = vec4(pow(finalColor, vec3(1.0/2.2)), albedo.a);
}
