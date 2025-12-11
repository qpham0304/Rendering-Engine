#pragma once

#include "./VulkanDevice.h"
#include "./VulkanSwapChain.h"
#include "../resources/buffers/UniformBufferVulkan.h"
#include "core/resources/managers/BufferManager.h"

class BufferVulkan;
class RenderDeviceVulkan;
struct Vertex;

class VulkanBufferManager : public BufferManager
{
public:
    std::vector<UniformBufferVulkan*> uniformbuffersList;

public:
    VulkanBufferManager(VulkanDevice& deviceRef);
    ~VulkanBufferManager();

    virtual void init() override;
    virtual void shutdown() override;
    virtual void destroy(uint32_t id) override;
    void bind(uint32_t id);
    BufferVulkan* getBuffer(uint32_t id);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    );

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool);
    uint32_t createVertexBuffer(const Vertex* vertices, int size, VkCommandPool commandPool);
    uint32_t createIndexBuffer(uint32_t* indices, int size, VkCommandPool commandPool);
    
    void createUniformBuffers(size_t bufferSize);

    uint32_t createUniformBuffer(VkBuffer& buffer, VkDeviceMemory& buffersMemory, size_t bufferSize);

    template<typename T>
    void updateUniformBuffer(uint32_t currentImage, T ubo) {
        uniformbuffersList[currentImage]->updateUniformBuffer(ubo);
    }


private:
    VulkanBufferManager(const VulkanBufferManager& other) = delete;
    VulkanBufferManager& operator=(const VulkanBufferManager& other) = delete;
    VulkanBufferManager(const VulkanBufferManager&& other) = delete;
    VulkanBufferManager& operator=(const VulkanBufferManager&& other) = delete;
    
    const uint32_t& _assignID();

private:
    VulkanDevice& device;
    RenderDeviceVulkan* renderDeviceVulkan;
    std::unordered_map<uint32_t, std::shared_ptr<BufferVulkan>> buffers;
};

