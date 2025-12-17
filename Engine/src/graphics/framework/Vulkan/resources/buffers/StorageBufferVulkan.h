#pragma once

#include "BufferVulkan.h"

class StorageBufferVulkan : public BufferVulkan
{
public:
	friend class BufferManagerVulkan;

	StorageBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory, size_t size);

	virtual ~StorageBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

	void update(const void* data, size_t size);

private:
	void* bufferMapped;
	size_t bufferSize;
};

