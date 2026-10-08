#include "BufferVulkan.h"
#include <volk.h>
#include <stdexcept>

BufferVulkan::BufferVulkan(uint32_t id, VkBuffer vkBuffer, VkDeviceMemory vkBufferMemory) 
	: Buffer(id), buffer(vkBuffer), bufferMemory(vkBufferMemory)
{

}

BufferVulkan::~BufferVulkan()
{

}

void BufferVulkan::update(const void *data, size_t size)
{
    memcpy(mappedBuffer, data, size);
}

const VkDeviceMemory &BufferVulkan::getMemory()
{
    return bufferMemory;
}

VkDeviceAddress BufferVulkan::getAddress()
{
    return deviceAddress;
}

BufferVulkan::operator VkBuffer() const
{
    return buffer;
}

void BufferVulkan::destroy(VkDevice device) {
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, bufferMemory, nullptr);
}