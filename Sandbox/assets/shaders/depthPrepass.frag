#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) flat in uint inObjIdx;

layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
} pc;

// depth only, must discard the same texels as gBuffer.frag since the gbuffer depth tests against this
void main() {
    Object obj = pc.objRef.objects[inObjIdx];
    Material mat = MaterialsBuffer(obj.materialsRef).m[MaterialIndicesBuffer(obj.materialIndicesRef).i[gl_PrimitiveID]];
    if (mat.alphaCutoff <= 0.0) {
        return;
    }

    float alpha = texture(samplerImages[nonuniformEXT(mat.albedoIdx)], inTexCoord + mat.uvOffset).a;
    if (alphaClipped(mat, alpha)) {
        discard;
    }
}
