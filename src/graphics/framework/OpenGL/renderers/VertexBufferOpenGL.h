#pragma once
#include "../../../renderers/VertexBuffer.h"



class VertexBufferOpenGL : public VertexBuffer
{
public:

	VertexBufferOpenGL();
	virtual ~VertexBufferOpenGL() override;

	virtual void create(const void* data, size_t size) override;
	virtual void destroy() override;
	virtual void bind() override;
	virtual void setAttribute(VertexAttribute attribute) override;

protected:
	unsigned int VAO;
	unsigned int VBO;

};