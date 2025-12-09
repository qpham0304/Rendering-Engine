#pragma once

#include "../../src/graphics/renderpasses/RenderPass.h"
#include "../core/VulkanDevice.h"

class VulkanRenderPass : RenderPass
{
public:
    VkRenderPass renderPass;


public:
    VulkanRenderPass(VulkanDevice& deviceRef);
    ~VulkanRenderPass();

    virtual void create() override;
    virtual void destroy() override;
    virtual void bind() override;
    


private:
    VulkanDevice& device;
};

