#pragma once
#include "Buffer.h"

class VertexBuffer : public Buffer
{
public:
	struct VertexAttribute
	{
		uint32_t location;
		uint32_t componentCount;
		enum class Type { Float, Int, UInt, Byte, UByte } type;
		size_t offset;
		size_t stride;
	};


	virtual ~VertexBuffer() = default;

	virtual void create(const void* data, size_t size) = 0;
	virtual void destroy() = 0;
	virtual void bind() = 0;
	virtual void setAttribute(VertexAttribute attribute) = 0;

protected:
	VertexBuffer() = default;

};

