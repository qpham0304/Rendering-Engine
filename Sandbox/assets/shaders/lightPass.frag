#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputPos;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inputNorm;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inputAlbedo;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 pos = subpassLoad(inputPos).rgb;
    vec3 norm = subpassLoad(inputNorm).rgb;
    vec4 albedo = subpassLoad(inputAlbedo);

    // If pos is 0,0,0 (cleared value), you might be hitting background
    if (length(norm) == 0.0) discard; 

    // Basic Ambient + Lambertian lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    float diff = max(dot(norm, lightDir), 0.1);
    
    outColor = vec4(albedo.rgb * diff, 1.0);
}