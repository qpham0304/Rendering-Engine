#ifndef VBO_CLASS_H
#define VBO_CLASS_H

#include<glad/glad.h>
#include "core/features/Object.h"

class VBO : public Object {
public:
	GLuint ID;
	VBO();
	VBO& operator=(const VBO& other);
	VBO(GLfloat* vertices, GLsizeiptr size);

	void Bind();
	void Unbind();
	void Delete();
};

#endif