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

const VkWriteDescriptorSetAccelerationStructureKHR& AccelStructureBufferVulkan::getDescAccelStructInfo()
{
    descASInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descASInfo.accelerationStructureCount = 1;
    descASInfo.pAccelerationStructures    = &accelStr;
    return descASInfo;
}

void AccelStructureBufferVulkan::destroy(VkDevice device)
{
    BufferVulkan::destroy(device);
    vkDestroyAccelerationStructureKHR(device, accelStr, nullptr);
}
