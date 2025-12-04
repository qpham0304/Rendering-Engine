#pragma once

#include "./BufferVulkan.h"

class IndexBufferVulkan : public BufferVulkan
{
public:

public:
	IndexBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);
	virtual ~IndexBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

private:

};