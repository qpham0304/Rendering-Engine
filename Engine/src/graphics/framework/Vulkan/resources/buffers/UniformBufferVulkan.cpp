#include "UniformBufferVulkan.h"
#include "vulkan/vulkan.h"

UniformBufferVulkan::UniformBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
}

UniformBufferVulkan::~UniformBufferVulkan()
{

}

void UniformBufferVulkan::bind(void* commandBuffer)
{

}
