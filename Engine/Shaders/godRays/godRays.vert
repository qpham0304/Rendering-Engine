#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 uv;
out vec2 lightPos2D;

uniform mat4 viewMatrix;

void main() {
    uv = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}