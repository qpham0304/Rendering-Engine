#pragma once
#include<glad/glad.h>
#include "VBO.h"
#include "core/features/Object.h"

class VAO : public Object
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

