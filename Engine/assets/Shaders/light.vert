#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexcoord;

out vec2 uv;
out vec3 normal;

uniform mat4 matrix;
uniform mat4 mvp;

void main() {
	uv = aTexcoord;
    mat3 normalMatrix = transpose(inverse(mat3(matrix)));
    normal = normalMatrix * aNormal;

	gl_Position = mvp * matrix * vec4(aPos, 1.0f);
}