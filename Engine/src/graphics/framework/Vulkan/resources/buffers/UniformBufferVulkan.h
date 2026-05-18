#pragma once

#include "BufferVulkan.h"

class UniformBufferVulkan : public BufferVulkan
{
public:
	friend class BufferManagerVulkan;

	UniformBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);

	virtual ~UniformBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

	void update(const void* data, size_t size);

	const VkDescriptorBufferInfo& getDescUniformBufferInfo();

private:
	void* uniformBufferMapped;	// mapped with bufferManager's gpu buffer

	VkDescriptorBufferInfo bufferInfo{};

};

