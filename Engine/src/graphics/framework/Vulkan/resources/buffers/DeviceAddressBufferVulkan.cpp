#include "DeviceAddressBufferVulkan.h"

DeviceAddressBufferVulkan::DeviceAddressBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
    
}

DeviceAddressBufferVulkan::~DeviceAddressBufferVulkan()
{

}

void DeviceAddressBufferVulkan::bind(void *commandBuffer)
{
    
}

void DeviceAddressBufferVulkan::update(const void *src, size_t size)
{
    memcpy(mappedBuffer, src, size);
}

uint64_t DeviceAddressBufferVulkan::getReference()
{
    return bufferAddressRef;
}
