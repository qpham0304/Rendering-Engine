#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) flat in uint inObjIdx;

layout(location = 0) out vec4 outColor;

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
    vec2 jitter;
    float mipBias;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 shadowTint;
    vec4 params;    // threshold, softness, brightness
} pc;

// currently have two tons lit or unlit
void main() {
    Object obj = pc.objRef.objects[inObjIdx];
    Material mat = MaterialsBuffer(obj.materialsRef).m[MaterialIndicesBuffer(obj.materialIndicesRef).i[gl_PrimitiveID]];

    vec2 matUV = materialUV(mat, inTexCoord, textureSize(samplerImages[nonuniformEXT(mat.albedoIdx)], 0));
    vec4 albedo = texture(samplerImages[nonuniformEXT(mat.albedoIdx)], matUV, ubo.mipBias);
    if (alphaClipped(mat, albedo.a)) {
        discard;
    }
    albedo *= mat.albedoFactor;

    // culling is off, the back face of thin geometry is shaded like its front
    vec3 N = normalize(inNormal);
    if (dot(N, ubo.cameraPos.xyz - inWorldPos) < 0.0) {
        N = -N;
    }

    // lit or shadow by the sun angle, the shadow side takes the tint instead of going dark
    float threshold = pc.params.x;
    float softness = max(pc.params.y, 1e-4);
    float NdotL = dot(N, normalize(pc.sunDirection.xyz));
    float lit = smoothstep(threshold - softness, threshold + softness, NdotL);
    vec3 light = mix(pc.shadowTint.rgb, vec3(1.0), lit);

    vec3 emissive = texture(samplerImages[nonuniformEXT(mat.emissiveIdx)], matUV, ubo.mipBias).rgb * mat.emissiveFactor;
    outColor = vec4(albedo.rgb * light * pc.sunColor.rgb / PI * pc.params.z + emissive, 1.0);
}
