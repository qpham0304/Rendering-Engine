#pragma once

#include "BufferVulkan.h"

class StorageBufferVulkan : public BufferVulkan
{
public:
	friend class BufferManagerVulkan;

	StorageBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory, size_t size);

	virtual ~StorageBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

	const VkDescriptorBufferInfo& getDescStorageBufferInfo();

private:
	VkDescriptorBufferInfo bufferInfo{};
	size_t bufferSize;
};

