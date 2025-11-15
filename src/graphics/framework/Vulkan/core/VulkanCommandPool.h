#pragma once

#include "VulkanDevice.h"

class VulkanCommandPool {
public:
    VulkanDevice& device;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t queueFamily;
    size_t bufferCount;

public:
    VulkanCommandPool(VulkanDevice& deviceRef);
    ~VulkanCommandPool();

    void create();
    void destroy();

    void beginBuffer(size_t index, VkCommandBufferUsageFlags usageFlags = 0);
    void endBuffer(size_t index);

private:
    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

};
