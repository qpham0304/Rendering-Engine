#pragma once

#include "graphics/renderers/Buffer.h"
#include "graphics/framework/Vulkan/core/WrapperStructs.h"

class BufferVulkan : public Buffer, protected VkWrap
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

    virtual void destroy(VkDevice device);

};

