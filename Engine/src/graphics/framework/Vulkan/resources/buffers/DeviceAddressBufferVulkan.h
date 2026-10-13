#pragma once

#include "BufferVulkan.h"

class DeviceAddressBufferVulkan : public BufferVulkan
{
public:
	friend class BufferManagerVulkan;

	DeviceAddressBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);

	virtual ~DeviceAddressBufferVulkan() override;

    virtual void bind(void* commandBuffer) override;
};

