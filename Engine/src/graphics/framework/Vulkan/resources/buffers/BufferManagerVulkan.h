#pragma once

#include "UniformBufferVulkan.h"
#include "../../core/VulkanDevice.h"
#include "../../core/VulkanSwapChain.h"
#include "../../core/VulkanUtils.h"
#include "core/resources/managers/BufferManager.h"

class BufferVulkan;
class RenderDeviceVulkan;

class BufferManagerVulkan : public BufferManager
{
public:
    BufferManagerVulkan();
    ~BufferManagerVulkan();

    virtual int init(WindowConfig config) override;
    virtual int onClose() override;
    virtual void destroy(uint32_t id) override;
    virtual void bind(uint32_t id) override;
    virtual uint32_t createVertexBuffer(const Vertex* vertices, int size) override;
    virtual uint32_t createIndexBuffer(const uint16_t* indices, int size) override;

    BufferVulkan* getBuffer(uint32_t id);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    );

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    void createUniformBuffers(std::vector<UniformBufferVulkan*>& uniformBuffers, size_t bufferSize);

    uint32_t createUniformBuffer(VkBuffer& buffer, VkDeviceMemory& buffersMemory, size_t bufferSize);

private:
    BufferManagerVulkan(const BufferManagerVulkan& other) = delete;
    BufferManagerVulkan& operator=(const BufferManagerVulkan& other) = delete;
    BufferManagerVulkan(const BufferManagerVulkan&& other) = delete;
    BufferManagerVulkan& operator=(const BufferManagerVulkan&& other) = delete;
    
    const uint32_t& _assignID();

private:
    RenderDeviceVulkan* renderDeviceVulkan;
    std::unordered_map<uint32_t, std::shared_ptr<BufferVulkan>> buffers;
};

