#version 460 core

layout (location = 0) in vec3 aPos;

uniform mat4 matrix;
uniform mat4 mvp;

void main()
{
	gl_Position = mvp * matrix * vec4(aPos, 1.0f);
}