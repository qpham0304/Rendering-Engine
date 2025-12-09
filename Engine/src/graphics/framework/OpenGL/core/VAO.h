#pragma once
#include<glad/glad.h>
#include "VBO.h"

class VAO
{
public:
	GLuint ID;
	VAO();
	VAO& operator=(const VAO& other);
	void LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset);
	void LinkVBO(VBO& VBO, GLuint layout);
	void Bind();
	void Unbind();
	void Delete();
};

