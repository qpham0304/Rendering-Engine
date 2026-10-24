#pragma once

#include "Buffer.h"
#include <vector>


class UniformBuffer : public Buffer
{
public:
	virtual ~UniformBuffer() = default;

	virtual void bind() = 0;


protected:
	UniformBuffer(uint32_t id) : Buffer(id) { setFootprint("UniformBuffer", sizeof(UniformBuffer), MemorySystem::Graphics); };

	std::vector<void*> uniformBuffersMapped;
};

