#pragma once
#include "../../../renderers/IndexBuffer.h"

class IndexBufferOpenGL : public IndexBuffer
{
public:
	IndexBufferOpenGL();
	virtual ~IndexBufferOpenGL() override;

	virtual void create(const void* data, size_t size) override;
	virtual void destroy() override;
	virtual void bind() override;

private:
	unsigned int EBO;

};