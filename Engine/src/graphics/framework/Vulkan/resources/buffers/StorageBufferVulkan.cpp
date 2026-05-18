#include "StorageBufferVulkan.h"
#include "vulkan/vulkan.h"
#include <cassert>
#include <cstring>

StorageBufferVulkan::StorageBufferVulkan(
    uint32_t id,
    VkBuffer buffer,
    VkDeviceMemory bufferMemory,
    size_t size
)
	: BufferVulkan(id, buffer, bufferMemory), bufferSize(size)
{
}

StorageBufferVulkan::~StorageBufferVulkan()
{

}

void StorageBufferVulkan::bind(void* commandBuffer)
{

}

void StorageBufferVulkan::update(const void *data, size_t size)
{
    memcpy(bufferMapped, data, size);
}

void* StorageBufferVulkan::getMappedBuffer() {
    return bufferMapped;
}

const VkDescriptorBufferInfo &StorageBufferVulkan::getDescStorageBufferInfo()
{
	bufferInfo.buffer = buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

    return bufferInfo;
}
