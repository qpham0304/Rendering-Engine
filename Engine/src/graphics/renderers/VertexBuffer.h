#pragma once
#include "Buffer.h"
#include <vector>
class VertexBuffer : public Buffer
{
public:
	struct VertexAttribute
	{
		uint32_t binding;
		uint32_t location;
		enum class Format { Float, Int, UInt, Byte, UByte } format;
		size_t offset;
	};

	virtual ~VertexBuffer() = default;

	virtual void bind(void* commandBuffer) override = 0;
	virtual void setAttributes(std::vector<VertexAttribute> attributes) = 0;

protected:
	VertexBuffer(uint32_t id) : Buffer(id) {};

};

