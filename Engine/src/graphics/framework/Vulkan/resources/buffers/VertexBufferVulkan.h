#pragma once

#include "./BufferVulkan.h"

class VertexBufferVulkan : public BufferVulkan
{
public:

public:
	VertexBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);

	virtual ~VertexBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;
	//virtual void setAttribute(VertexAttribute attribute);

private:

};