#include "UniformBufferVulkan.h"
#include "vulkan/vulkan.h"
#include <cstring>

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

void UniformBufferVulkan::update(const void* data, size_t size)
{
	memcpy(uniformBufferMapped, data, size);
}

const VkDescriptorBufferInfo& UniformBufferVulkan::getDescUniformBufferInfo()
{
	bufferInfo.buffer = buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

    return bufferInfo;
}
