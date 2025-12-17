#pragma once

#include "VulkanDevice.h"

class VulkanCommandPool {

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
    void drawIndexed(uint32_t indexCount, uint32_t firstInstance = 0);
    void drawInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t firstInstance = 0);
    VkCommandBuffer currentBuffer();

private:
    void createCommandPool();
    void createCommandBuffers();

private:
    VulkanDevice& device;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t queueFamily;


};
