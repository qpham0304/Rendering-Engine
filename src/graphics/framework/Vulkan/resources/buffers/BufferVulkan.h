#pragma once

#include "src/graphics/renderers/Buffer.h"
#include "src/graphics/framework/Vulkan/core/WrapperStructs.h"

class BufferVulkan : public Buffer, protected VkWrap
{
public:
    friend class VulkanBufferManager;

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

