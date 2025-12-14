#pragma once

#include "graphics/renderers/Buffer.h"
#include "graphics/framework/Vulkan/core/WrapperStructs.h"

class BufferVulkan : public Buffer, protected VkWrap
{
public:
    friend class BufferManagerVulkan;

    BufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);

    virtual ~BufferVulkan() override;

    explicit operator VkBuffer() const;

    virtual void bind(void* commandBuffer) override;

    const VkDeviceMemory& getMemory();


protected:
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;


private:
    void destroy(VkDevice device);

};

