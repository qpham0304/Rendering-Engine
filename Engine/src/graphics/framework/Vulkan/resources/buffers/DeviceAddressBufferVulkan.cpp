#include "DeviceAddressBufferVulkan.h"

DeviceAddressBufferVulkan::DeviceAddressBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
    
}

DeviceAddressBufferVulkan::~DeviceAddressBufferVulkan()
{

}

void DeviceAddressBufferVulkan::bind(void *commandBuffer)
{
    
}