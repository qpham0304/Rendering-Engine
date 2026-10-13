#pragma once

#include "./BufferVulkan.h"

class IndexBufferVulkan : public BufferVulkan
{
public:

public:
	IndexBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory);
	virtual ~IndexBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

private:

};