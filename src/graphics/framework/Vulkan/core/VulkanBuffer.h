#pragma once

#include "../../../renderers/Buffer.h"
#include "./VulkanDevice.h"

class VulkanBuffer : public Buffer
{
public:
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkBuffer combinedBuffer;
    VkDeviceMemory combinedBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

public:
    VulkanBuffer(VulkanDevice& deviceRef);
    ~VulkanBuffer();

    virtual void create() override;
    virtual void destroy() override;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    );

    void createVertexBuffer(std::vector<VulkanDevice::Vertex> vertices, VkCommandPool commandPool);
    void createIndexBuffer(std::vector<uint16_t> indices, VkCommandPool commandPool);
    void createCombinedBuffer(std::vector<VulkanDevice::Vertex> vertices, std::vector<uint16_t> indices, VkCommandPool commandPool);
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentImage, VulkanDevice::UniformBufferObject ubo);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool);


private:
    VulkanBuffer(const VulkanBuffer& other) = delete;
    VulkanBuffer& operator=(const VulkanBuffer& other) = delete;
    VulkanBuffer(const VulkanBuffer&& other) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&& other) = delete;


private:
    VulkanDevice& device;

};

