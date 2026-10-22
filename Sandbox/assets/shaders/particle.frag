#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/particles.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) flat in int fragTextureID;
layout(location = 4) flat in float fragShading;

layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

const vec3 VIEW_LIGHT_DIR = vec3(0.3, 0.8, 0.5);    // headlight style, meshes only need a hint of shape

void main() {
    vec4 color = fragColor;

    if(fragTextureID != 0) {
        color *= texture(samplerImages[nonuniformEXT(fragTextureID)], fragTexCoord);
    }

    if(dot(fragNormal, fragNormal) > 0.0) {
        float lambert = 0.35 + 0.65 * max(dot(normalize(fragNormal), normalize(VIEW_LIGHT_DIR)), 0.0);
        color.rgb *= mix(1.0, lambert, fragShading);
    }

    if(color.a <= 0.003) {
        discard;
    }

    outColor = color;
}
