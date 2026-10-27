#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) flat in uint inObjIdx;

layout(location = 0) out vec4 outMoment;

layout(set = 0, binding = 0) uniform sampler2D samplerImages[];

layout(push_constant) uniform MeshData {
    mat4 lightMVP;
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
} push;

void main() {
    Object obj = push.objRef.objects[inObjIdx];
    Material mat = MaterialsBuffer(obj.materialsRef).m[MaterialIndicesBuffer(obj.materialIndicesRef).i[gl_PrimitiveID]];
    if (mat.alphaCutoff > 0.0) {
        float alpha = texture(samplerImages[nonuniformEXT(mat.albedoIdx)], inTexCoord + mat.uvOffset).a;
        if (alphaClipped(mat, alpha)) {
            discard;
        }
    }

	float d = gl_FragCoord.z;
    // d = d * 2.0 - 1.0;
	float d2 = d * d;
	float d3 = d2 * d;
	float d4 = d3 * d;

	outMoment = vec4(d, d2, d3, d4);
}
