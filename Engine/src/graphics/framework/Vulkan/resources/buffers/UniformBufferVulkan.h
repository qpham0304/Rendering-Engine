#pragma once

#include "BufferVulkan.h"

class UniformBufferVulkan : public BufferVulkan
{
public:
	friend class BufferManagerVulkan;

	UniformBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);

	virtual ~UniformBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

	template<typename T>
	void updateUniformBuffer(T ubo) {
		memcpy(uniformBufferMapped, &ubo, sizeof(T));
	}

private:
	void* uniformBufferMapped;

};

