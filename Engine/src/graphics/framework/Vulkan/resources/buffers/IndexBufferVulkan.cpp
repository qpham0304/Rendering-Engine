#include "IndexBufferVulkan.h"
#include <volk.h>

IndexBufferVulkan::IndexBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
	setFootprint("IndexBufferVulkan", sizeof(IndexBufferVulkan), MemorySystem::Graphics);
}

IndexBufferVulkan::~IndexBufferVulkan()
{

}

void IndexBufferVulkan::bind(void* commandBuffer)
{
	vkCmdBindIndexBuffer((VkCommandBuffer)commandBuffer, buffer, 0, VK_INDEX_TYPE_UINT32);

}
