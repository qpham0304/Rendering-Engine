#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include "graphics/renderers/Buffer.h"

class BufferVulkan : public Buffer
{
public:
    friend class BufferManagerVulkan;

    BufferVulkan() { setFootprint("BufferVulkan", sizeof(BufferVulkan), MemorySystem::Graphics); };
    BufferVulkan(uint32_t id, VkBuffer buffer, VmaAllocation bufferMemory);

    virtual ~BufferVulkan() override;

    explicit operator VkBuffer() const;

    virtual void bind(void* commandBuffer) override {};
	virtual void update(const void* data, size_t size) override;

    const VmaAllocation& getMemory();

	VkDeviceAddress getAddress();

    BufferVulkan(BufferVulkan&& other) noexcept = default;
    BufferVulkan& operator=(BufferVulkan&& other) noexcept = default;

    BufferVulkan(const BufferVulkan&) = delete;
    BufferVulkan& operator=(const BufferVulkan&) = delete;


protected:
    VkBuffer buffer { VK_NULL_HANDLE };
    VmaAllocation bufferMemory { VK_NULL_HANDLE };
	VkDeviceAddress deviceAddress { 0 };

    virtual void destroy(VkDevice device, VmaAllocator allocator);
        
    void* mappedBuffer { nullptr };

};

