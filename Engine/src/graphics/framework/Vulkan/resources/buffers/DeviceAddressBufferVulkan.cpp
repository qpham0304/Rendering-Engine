#include "DeviceAddressBufferVulkan.h"

DeviceAddressBufferVulkan::DeviceAddressBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
    setFootprint("DeviceAddressBufferVulkan", sizeof(DeviceAddressBufferVulkan), MemorySystem::Graphics);
    
}

DeviceAddressBufferVulkan::~DeviceAddressBufferVulkan()
{

}

void DeviceAddressBufferVulkan::bind(void *commandBuffer)
{
    
}