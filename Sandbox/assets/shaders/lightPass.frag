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

layout (push_constant) uniform LightData {
    int numLights;
} pcl;

void main() {
    // 1. Sample G-Buffer as usual
    vec3 worldPos  = subpassLoad(inputPos).rgb;
    vec3 worldNorm = normalize(subpassLoad(inputNorm).rgb);
    vec4 albedo    = subpassLoad(inputAlbedo);
    vec3 pbr       = subpassLoad(inputPBR).rgb; 

    if (length(worldNorm) < 0.1) discard;

    float ao        = pbr.r;
    vec3 camPos     = ubo.cameraPos.xyz;
    vec3 V          = normalize(camPos - worldPos);
    
    // --- LIGHT ACCUMULATION START ---
    vec3 accumulatedLight = vec3(0.0);
    vec3 ambient = vec3(0.03) * albedo.rgb * ao;

    // Assuming you pass the number of lights via Push Constant
    // because the SSBO doesn't inherently know its own length in all GLSL versions
    for(int i = 0; i < pcl.numLights; i++) {
        vec3 lightPos   = lightSSBO.lights[i].position.xyz;
        vec3 lightCol   = lightSSBO.lights[i].color.rgb; // This is 'lightCol'
        float intensity = lightSSBO.lights[i].intensity;
        float radius    = lightSSBO.lights[i].radius;

        vec3 L = normalize(lightPos - worldPos);
        float dist = length(lightPos - worldPos);

        if (dist > radius) {
            continue;
        }

        // Standard attenuation
        float attenuation = intensity / (dist * dist + 0.01);

        float factor = dist / radius;
        float smoothFactor = clamp(1.0 - factor * factor * factor * factor, 0.0, 1.0);
        attenuation *= (smoothFactor * smoothFactor);
        float diffuse = max(dot(worldNorm, L), 0.0);

        accumulatedLight += (albedo.rgb * diffuse * lightCol * attenuation);
    }

    vec3 finalColor = ambient + accumulatedLight;
    outColor = vec4(pow(finalColor, vec3(1.0/2.2)), 1.0);
}