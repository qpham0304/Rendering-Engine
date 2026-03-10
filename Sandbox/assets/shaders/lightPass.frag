#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inputNorm;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inputAlbedo;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput inputPBR;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0)  uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

struct Light {
    vec4 color;      
    vec4 position;
    float intensity;
    float radius;
};

layout(set = 1, binding = 1, std430) readonly buffer LightSSBO {
    Light lights[];
} lightSSBO;

layout(set = 1, binding = 2) uniform sampler2D shadowMap;
layout(set = 1, binding = 3) uniform sampler2D blueNoise;

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

    // --- STEP 1: BLOCKER SEARCH ---
    float avgBlockerDepth = 0.0;
    int blockers = 0;
    
    // LIGHT_SIZE_UV: How big is the light source? 
    // Increase this to 0.1 or 0.2 to force a massive, obvious blur for testing.
    const float LIGHT_SIZE_UV = 0.05; 
    
    // We need a wide search to find blockers that are far away
    float searchRegion = LIGHT_SIZE_UV * (currentDepth); 

    // Use a small grid for blocker search
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

    // --- STEP 2: PENUMBRA ESTIMATION ---
    // The ratio of (Receiver - Blocker) / Blocker
    // float penumbra = (currentDepth - avgBlockerDepth) * LIGHT_SIZE_UV / avgBlockerDepth;
    float penumbra = (currentDepth - avgBlockerDepth) * LIGHT_SIZE_UV;
    
    // CLAMP: Ensure it doesn't get so big that the shadow disappears into noise
    penumbra = clamp(penumbra, 0.0, 0.02); 

    vec2 noiseUV = gl_FragCoord.xy / vec2(textureSize(blueNoise, 0));
    // noiseUV += vec2(pcl.time * 0.1337, pcl.time * 0.4337);
    float noiseValue = texture(blueNoise, noiseUV).r;
    // float noiseValue = random(worldPos + vec3(pcl.time));

    // --- STEP 3: FILTERING (PCF) ---
    // Generate a random angle based on pixel position
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
    // hard comparison to stop bleeding
    for (int i = 0; i < 16; i++) {
        vec2 offset = (rotation * poissonDisk32[i]) * penumbra;
        float pcfDepth = texture(shadowMap, projCoords.xy + offset).r;
        
        // Strict comparison = No bleeding
        if (currentDepth - 0.0015 > pcfDepth) {
            shadow += 1.0;
        }
    }
    return shadow / 16.0;
}

void main() {
    vec3 worldPos  = subpassLoad(inputPos).rgb;
    vec3 worldNorm = normalize(subpassLoad(inputNorm).rgb);
    vec4 albedo    = subpassLoad(inputAlbedo);
    vec3 pbr       = subpassLoad(inputPBR).rgb; 

    if (length(worldNorm) < 0.1) discard;

    float ao        = pbr.r;
    vec3 camPos     = ubo.cameraPos.xyz;
    vec3 V          = normalize(camPos - worldPos);
    
    vec3 accumulatedLight = vec3(0.0);
    vec3 ambient = vec3(0.03) * albedo.rgb * ao;

    for(int i = 0; i < pcl.numLights ; i++) {
        vec3 lightPos   = lightSSBO.lights[i].position.xyz;
        vec3 lightCol   = lightSSBO.lights[i].color.rgb;
        float intensity = lightSSBO.lights[i].intensity;
        float radius    = lightSSBO.lights[i].radius;

        vec3 L = normalize(lightPos - worldPos);
        float dist = length(lightPos - worldPos);

        if (dist > radius) {
            continue;
        }

        float attenuation = intensity / (dist * dist + 0.01);

        float factor = dist / radius;
        float smoothFactor = clamp(1.0 - factor * factor * factor * factor, 0.0, 1.0);
        attenuation *= (smoothFactor * smoothFactor);
        float diffuse = max(dot(worldNorm, L), 0.0);

        accumulatedLight += (albedo.rgb * diffuse * lightCol * attenuation);
    }
    
    // float shadow = calcShadow(worldPos);
    // float shadow = calcMSMShadow(worldPos + worldNorm * pcl.bias);
    float shadow = calcPCSS(worldPos);
    
    vec3 sunDir = normalize(pcl.direction.xyz); 
    float sunDiffuse = max(dot(worldNorm, sunDir), 0.0);
    vec3 sunlight = (1.0 - shadow) * (pcl.color.rgb * sunDiffuse * albedo.rgb);

    vec3 finalColor = ambient + accumulatedLight + sunlight;
    outColor = vec4(pow(finalColor, vec3(1.0/2.2)), 1.0);
}