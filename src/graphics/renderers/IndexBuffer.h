#pragma once
#include "Buffer.h"

class IndexBuffer : public Buffer
{
public:
	struct IndexAttribute
	{
		uint32_t location;
		uint32_t componentCount;
		enum class Type { Float, Int, UInt, Byte, UByte } type;
		size_t offset;
		size_t stride;
	};

	virtual ~IndexBuffer() = default;

	virtual void bind(void* commandBuffer) override = 0;

protected:
	IndexBuffer(uint32_t id) : Buffer(id) {};
};

