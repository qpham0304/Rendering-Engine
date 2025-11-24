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

    void beginBuffer(VkCommandBufferUsageFlags usageFlags = 0);
    void endBuffer();

    VkCommandBuffer beginSingleTimeCommand();
    void endSingleTimeCommand(VkCommandBuffer commandBuffer);

    void draw(uint32_t verticesCount);
    void drawIndexed(uint32_t indexCount);
    void drawInstanced(uint32_t indexCount, uint32_t instanceCount);
    VkCommandBuffer currentBuffer();

private:
    void createCommandPool();
    void createCommandBuffers();
    //void recordCommandBuffer(uint32_t imageIndex);

};
