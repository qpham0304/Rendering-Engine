#include "VertexBufferOpenGL.h"
#include<glad/glad.h>

typedef VertexBuffer::VertexAttribute VertexAttribute;

static GLenum toGLType(VertexAttribute::Type type)
{
	switch (type)
	{
		case VertexAttribute::Type::Float: return GL_FLOAT;
		case VertexAttribute::Type::Int:   return GL_INT;
		case VertexAttribute::Type::UInt:  return GL_UNSIGNED_INT;
		case VertexAttribute::Type::Byte:  return GL_BYTE;
		case VertexAttribute::Type::UByte: return GL_UNSIGNED_BYTE;
	}
	return GL_FLOAT;
}

VertexBufferOpenGL::VertexBufferOpenGL() : VAO(0), VBO(0)
{

}

VertexBufferOpenGL::~VertexBufferOpenGL()
{

}

void VertexBufferOpenGL::create(const void* data, size_t size)
{
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

void VertexBufferOpenGL::destroy()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
}

void VertexBufferOpenGL::bind()
{
	glBindVertexArray(VAO);
	glDeleteBuffers(1, &VBO);
}

void VertexBufferOpenGL::setAttribute(VertexAttribute attribute)
{
	bind();
	glEnableVertexAttribArray(attribute.location);
	glVertexAttribPointer(
		attribute.location, 
		attribute.componentCount, 
		toGLType(attribute.type), 
		GL_FALSE, 
		attribute.stride, 
		(void*)attribute.offset
	);
}
