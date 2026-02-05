#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inWorldPos; 

// These map directly to the 3 references in your subpass0.pColorAttachments
layout(location = 0) out vec4 outPos;    // Maps to gBufferReferences[0] (Index 1)
layout(location = 1) out vec4 outNorm;   // Maps to gBufferReferences[1] (Index 2)
layout(location = 2) out vec4 outAlbedo; // Maps to gBufferReferences[2] (Index 3)

layout(set = 1, binding = 0) uniform sampler2D albedoSampler;

void main() {
    outPos    = vec4(inWorldPos, 1.0);
    outNorm   = vec4(normalize(inNormal), 1.0);
    outAlbedo = texture(albedoSampler, inTexCoord);
}