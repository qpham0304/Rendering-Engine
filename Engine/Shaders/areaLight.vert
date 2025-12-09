#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexcoord;

uniform mat4 matrix;
uniform mat3 normalMatrix;
uniform mat4 mvp;

out vec3 worldPos;
out vec3 worldNormal;
out vec2 uv;
out vec4 clipSpace;

void main()
{
	vec4 worldpos = matrix * vec4(aPosition, 1.0f);
	worldPos = worldpos.xyz;
	worldNormal = normalMatrix * aNormal;
	uv = aTexcoord;

	clipSpace = mvp * worldpos;
	gl_Position = clipSpace;
}
