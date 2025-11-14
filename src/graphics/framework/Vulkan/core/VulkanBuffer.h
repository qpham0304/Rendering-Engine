#pragma once

#include "../../../renderers/Buffer.h"
class VulkanBuffer : public Buffer
{
public:
    VulkanBuffer();
    ~VulkanBuffer();

    virtual void create() override;
    virtual void destroy() override;


private:
    VulkanBuffer(const VulkanBuffer& other) = delete;
    VulkanBuffer& operator=(const VulkanBuffer& other) = delete;
    VulkanBuffer(const VulkanBuffer&& other) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&& other) = delete;

    void createVertexBuffer();
    void createIndexBuffer();
    void createCombinedBuffer();
    void createCommandBuffers();

private:

};

