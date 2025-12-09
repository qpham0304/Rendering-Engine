#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aNormal;
layout (location = 4) in vec3 tangent;
layout (location = 5) in vec3 bitangent;
layout (location = 6) in ivec4 boneIds;
layout (location = 7) in vec4 weights;

out vec2 uv;
out vec3 normal;
out vec3 fragPos;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

uniform bool invertedNormals = false;
uniform bool invertedTexCoords = false;
uniform mat4 modelViewNormal;
uniform mat4 mvp;
uniform bool hasAnimation = false;

void main() {
    uv = aTexCoords;
    invertedTexCoords ? uv.y *= -1 : uv.y; // use this to flip texture horizontally
    
    int invertNormal = invertedNormals ? -1 : 1;
    normal = mat3(modelViewNormal) * aNormal * invertNormal; //view space normal
    
    vec4 totalPosition = vec4(0.0f);
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
        if(boneIds[i] == -1) {
            continue;
        }

        if(boneIds[i] >= MAX_BONES)  {
            totalPosition = vec4(aPos,1.0f);
            break;
        }

        vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(aPos,1.0f);
        totalPosition += localPosition * weights[i];
        vec3 localNormal = mat3(finalBonesMatrices[boneIds[i]]) * normal;
    }

    
    int condition = int(hasAnimation);
    totalPosition = condition * totalPosition + (1 - condition) * vec4(aPos, 1.0f);
    fragPos = totalPosition.xyz;
    
    gl_Position = mvp * totalPosition;
}