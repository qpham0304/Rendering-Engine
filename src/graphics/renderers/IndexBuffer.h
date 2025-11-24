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

	virtual void create(const void* data, size_t size) override = 0;
	virtual void destroy() override = 0;
	virtual void bind() override = 0;

protected:
	IndexBuffer() = default;

};

