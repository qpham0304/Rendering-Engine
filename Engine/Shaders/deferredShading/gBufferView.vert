#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 uv;
out vec3 normal;

uniform bool invertedNormals = false;
uniform bool invertedTexCoords = false;
uniform mat4 modelViewNormal;
uniform mat4 mvp;

void main() {
    uv = aTexCoords;
    invertedTexCoords ? uv.y *= -1 : uv.y; // TODO: only use this to flip texture horizontally
    normal = mat3(modelViewNormal) * (invertedNormals ? -aNormal : aNormal);
    gl_Position = mvp * vec4(aPos, 1.0);
}