#pragma once

#include "BufferVulkan.h"

class UniformBufferVulkan : public BufferVulkan
{
public:
	friend class BufferManagerVulkan;

	UniformBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory);

	virtual ~UniformBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

	const VkDescriptorBufferInfo& getDescUniformBufferInfo();

private:
	VkDescriptorBufferInfo bufferInfo{};

};

