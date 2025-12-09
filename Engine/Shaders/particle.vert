#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexcoord;
layout (location = 3) in mat4 aInstanceMatrix;

uniform mat4 mvp;

void main() {
	gl_Position = mvp * aInstanceMatrix * vec4(aPos, 1.0f);
}