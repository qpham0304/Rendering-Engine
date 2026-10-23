#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

layout(push_constant) uniform RibbonPushConstant {
    uint64_t pointsRef;
    uint firstPoint;
    uint pointCount;
    int textureID;
    uint flags;
} pc;

const uint STRIP_FLAG_PAIRED = 1u;

void main() {
    vec4 color = fragColor;

    if (pc.textureID != 0) {
        color *= texture(samplerImages[nonuniformEXT(pc.textureID)], fragTexCoord);
    } else if ((pc.flags & STRIP_FLAG_PAIRED) != 0u) {  // slash stays sharp at the tip edge and fades toward the hilt
        color.a *= smoothstep(0.0, 0.8, fragTexCoord.y);
    } else {    // without a texture the edges soften so a plain ribbon reads as a glow instead of a flat band
        float acrossCenter = 1.0 - abs(fragTexCoord.y * 2.0 - 1.0);
        color.a *= smoothstep(0.0, 0.6, acrossCenter);
    }

    if (color.a <= 0.003) {
        discard;
    }

    outColor = color;
}
