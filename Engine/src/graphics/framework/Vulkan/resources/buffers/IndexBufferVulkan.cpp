#include "IndexBufferVulkan.h"
#include "vulkan/vulkan.h"

IndexBufferVulkan::IndexBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
}

IndexBufferVulkan::~IndexBufferVulkan()
{

}

void IndexBufferVulkan::bind(void* commandBuffer)
{
	vkCmdBindIndexBuffer((VkCommandBuffer)commandBuffer, buffer, 0, VK_INDEX_TYPE_UINT16);

}
