#include "UniformBufferVulkan.h"
#include <volk.h>
#include <cstring>

UniformBufferVulkan::UniformBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
}

UniformBufferVulkan::~UniformBufferVulkan()
{

}

void UniformBufferVulkan::bind(void* commandBuffer)
{

}

const VkDescriptorBufferInfo& UniformBufferVulkan::getDescUniformBufferInfo()
{
	bufferInfo.buffer = buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

    return bufferInfo;
}
