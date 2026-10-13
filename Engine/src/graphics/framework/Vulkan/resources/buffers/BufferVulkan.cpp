#include "BufferVulkan.h"
#include <volk.h>
#include <stdexcept>

BufferVulkan::BufferVulkan(uint32_t id, VkBuffer vkBuffer, VmaAllocation vkBufferMemory) 
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

const VmaAllocation &BufferVulkan::getMemory()
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

void BufferVulkan::destroy(VkDevice device, VmaAllocator allocator) {
    if (buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, buffer, bufferMemory);
        buffer = VK_NULL_HANDLE;
        bufferMemory = VK_NULL_HANDLE;
    }
}