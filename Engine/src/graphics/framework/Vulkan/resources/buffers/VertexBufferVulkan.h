#pragma once

#include "./BufferVulkan.h"

class VertexBufferVulkan : public BufferVulkan
{
public:

public:
	VertexBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory);

	virtual ~VertexBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

private:

};