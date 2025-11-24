#include "IndexBufferOpenGL.h"
#include<glad/glad.h>

IndexBufferOpenGL::IndexBufferOpenGL() : EBO(0)
{
}

IndexBufferOpenGL::~IndexBufferOpenGL()
{

}

void IndexBufferOpenGL::create(const void* data, size_t size)
{
	glGenBuffers(1, &EBO);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

void IndexBufferOpenGL::destroy()
{
	glDeleteBuffers(1, &EBO);
}

void IndexBufferOpenGL::bind()
{

}
