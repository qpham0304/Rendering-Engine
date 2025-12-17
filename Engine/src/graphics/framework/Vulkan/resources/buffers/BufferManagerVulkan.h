#pragma once

#include "UniformBufferVulkan.h"
#include "StorageBufferVulkan.h"
#include "../../core/VulkanDevice.h"
#include "../../core/VulkanSwapChain.h"
#include "../../core/VulkanUtils.h"
#include "core/resources/managers/BufferManager.h"

class BufferVulkan;
class RenderDeviceVulkan;

class BufferManagerVulkan : public BufferManager
{
public:
    BufferManagerVulkan(std::string serviceName = "BufferManagerVulkan");
    ~BufferManagerVulkan();

    virtual int init(WindowConfig config) override;
    virtual int onClose() override;
    virtual void destroy(uint32_t id) override;
    virtual std::vector<uint32_t> listIDs() const override {
		std::vector<uint32_t> list;
		for(const auto& [id, buffer] : buffers) {
			list.emplace_back(id);
		}
		return list;
	}
    virtual void bind(uint32_t id) override;
    virtual uint32_t createVertexBuffer(const Vertex* vertices, int size) override;
    virtual uint32_t createIndexBuffer(const uint16_t* indices, int size) override;
    virtual BufferVulkan* getBuffer(uint32_t id) override;


    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    );

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void copyBuffer(    
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        VkDeviceSize size,
        VkDeviceSize srcOffset,
        VkDeviceSize dstOffset
    );
    
    //TODO: include these buffer creations in the interface so they can be used
    //TODO: redesign a more consistent api for these instead of require VulkanTypes
    void createUniformBuffers(std::vector<UniformBufferVulkan*>& uniformBuffers, size_t bufferSize);
    uint32_t createUniformBuffer(VkBuffer& buffer, VkDeviceMemory& buffersMemory, size_t bufferSize);

    void createStorageBuffers(std::vector<StorageBufferVulkan*>& storageBuffers, size_t bufferSize);
    uint32_t createStorageBuffer(VkBuffer& buffer, VkDeviceMemory& buffersMemory, size_t bufferSize);
    //void updateStorageBuffer(uint32_t id, const void* srcData, size_t size, size_t offset);

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

