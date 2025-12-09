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

void BufferVulkan::bind(void* commandBuffer)
{
    throw std::runtime_error("unimplemented error");
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