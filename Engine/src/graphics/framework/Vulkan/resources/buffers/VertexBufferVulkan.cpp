#include "VertexBufferVulkan.h"
#include <volk.h>

VertexBufferVulkan::VertexBufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
}

VertexBufferVulkan::~VertexBufferVulkan()
{

}

void VertexBufferVulkan::bind(void* commandBuffer)
{
	VkBuffer vertexBuffers[] = { buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers((VkCommandBuffer)commandBuffer, 0, 1, vertexBuffers, offsets);
}
