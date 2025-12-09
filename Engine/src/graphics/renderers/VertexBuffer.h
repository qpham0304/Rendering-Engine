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

	virtual void bind() override = 0;
	virtual void setAttribute(VertexAttribute attribute) = 0;

protected:
	VertexBuffer(uint32_t id) : Buffer(id) {};

};

