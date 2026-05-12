#include "AccelStructureBufferVulkan.h"
#include "vulkan/vulkan.h"
#include <cstring>

AccelStructureBufferVulkan::AccelStructureBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
}

AccelStructureBufferVulkan::~AccelStructureBufferVulkan()
{

}

void AccelStructureBufferVulkan::bind(void* commandBuffer)
{

}

uint64_t AccelStructureBufferVulkan::getAddress()
{
    return deviceAddress;
}

VkAccelerationStructureKHR AccelStructureBufferVulkan::getAccelStr()
{
    return accelStr;
}

void AccelStructureBufferVulkan::destroy(VkDevice device)
{
    BufferVulkan::destroy(device);
    vkDestroyAccelerationStructureKHR(device, accelStr, nullptr);
}

// void AccelStructureBufferVulkan::update(const void* data, size_t size)
// {
// 	memcpy(uniformBufferMapped, data, size);
// }
