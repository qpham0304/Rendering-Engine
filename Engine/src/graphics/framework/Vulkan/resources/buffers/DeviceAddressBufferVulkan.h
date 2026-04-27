#pragma once

#include "BufferVulkan.h"

class DeviceAddressBufferVulkan : public BufferVulkan
{
public:
	friend class BufferManagerVulkan;

	DeviceAddressBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);

	virtual ~DeviceAddressBufferVulkan() override;

    virtual void bind(void* commandBuffer) override;

	void update(const void* src, size_t size);

	uint64_t getReference();
	
private:
	uint64_t bufferAddressRef;
	void* mappedBuffer;
};

