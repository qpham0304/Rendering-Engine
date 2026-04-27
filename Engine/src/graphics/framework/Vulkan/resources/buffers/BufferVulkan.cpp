#include "BufferVulkan.h"
#include "vulkan/vulkan.h"
#include <stdexcept>

BufferVulkan::BufferVulkan(uint32_t id, VkBuffer vkBuffer, VkDeviceMemory vkBufferMemory) 
	: Buffer(id), buffer(vkBuffer), bufferMemory(vkBufferMemory)
{

}

BufferVulkan::~BufferVulkan()
{

}

const VkDeviceMemory& BufferVulkan::getMemory()
{
    return bufferMemory;
}

BufferVulkan::operator VkBuffer() const
{
    return buffer;
}

void BufferVulkan::destroy(VkDevice device) {
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, bufferMemory, nullptr);
}