// #version 460 core

// out vec4 FragColor;

// in vec2 uv;

// uniform sampler2D scene;
// uniform sampler2D bloomBlur;
// uniform float exposure;
// uniform float bloomStrength = 0.04f;
// uniform bool enable = true;

// vec3 bloom_off() {
//     vec3 hdrColor = texture(scene, uv).rgb;
//     return hdrColor;
// }

// vec3 bloom_on() {
//     vec3 hdrColor = texture(scene, uv).rgb;
//     vec3 bloomColor  = texture(bloomBlur, uv).rgb;
//     return mix(hdrColor, bloomColor, bloomStrength);
// }

// void main() {
//     int isOn = enable ? 1 : 0;  //todo, should be casting enable isntead
//     vec3 result = mix(bloom_on(), bloom_off(), isOn);
//     result = vec3(1.0) - exp(-result * exposure);   // hdr
//     const float gamma = 2.2;
//     result = pow(result, vec3(1.0 / gamma));    // gamma
//     FragColor = vec4(result, 1.0);
// }
 
#version 330 core
out vec4 FragColor;

in vec2 uv;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float exposure;
uniform float bloomStrength = 0.04f;
uniform bool bloomOn = true;

vec3 bloom_none()
{
    vec3 hdrColor = texture(scene, uv).rgb;
    return hdrColor;
}

vec3 bloom_new()
{
    vec3 hdrColor = texture(scene, uv).rgb;
    vec3 bloomColor = texture(bloomBlur, uv).rgb;
    return mix(hdrColor, bloomColor, bloomStrength); // linear interpolation
}

void main()
{
    vec3 result = bloomOn ? bloom_new() : bloom_none();
    result = vec3(1.0) - exp(-result * exposure);
    const float gamma = 2.2;
    result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, 1.0);
}
