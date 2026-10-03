#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include "graphics/renderers/Buffer.h"

class BufferVulkan : public Buffer
{
public:
    friend class BufferManagerVulkan;

    BufferVulkan() = default;
    BufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);

    virtual ~BufferVulkan() override;

    explicit operator VkBuffer() const;

    virtual void bind(void* commandBuffer) override {};

    const VkDeviceMemory& getMemory();

	VkDeviceAddress getAddress();

    BufferVulkan(BufferVulkan&& other) noexcept = default;
    BufferVulkan& operator=(BufferVulkan&& other) noexcept = default;

    BufferVulkan(const BufferVulkan&) = delete;
    BufferVulkan& operator=(const BufferVulkan&) = delete;


protected:
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;
	VkDeviceAddress deviceAddress;
    VmaAllocator m_allocator;
    VmaAllocation m_allocation { VK_NULL_HANDLE };

    virtual void destroy(VkDevice device);
        
    void* mappedBuffer { nullptr };

};

