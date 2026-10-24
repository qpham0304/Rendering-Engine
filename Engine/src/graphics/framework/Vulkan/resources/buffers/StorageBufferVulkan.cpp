#include "StorageBufferVulkan.h"
#include <volk.h>
#include <cassert>
#include <cstring>

StorageBufferVulkan::StorageBufferVulkan(
    uint32_t id,
    VkBuffer buffer,
    VmaAllocation bufferMemory,
    size_t size
)
	: BufferVulkan(id, buffer, bufferMemory), bufferSize(size)
{
    setFootprint("StorageBufferVulkan", sizeof(StorageBufferVulkan), MemorySystem::Graphics);
}

StorageBufferVulkan::~StorageBufferVulkan()
{

}

void StorageBufferVulkan::bind(void* commandBuffer)
{

}

const VkDescriptorBufferInfo& StorageBufferVulkan::getDescStorageBufferInfo()
{
	bufferInfo.buffer = buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

    return bufferInfo;
}
