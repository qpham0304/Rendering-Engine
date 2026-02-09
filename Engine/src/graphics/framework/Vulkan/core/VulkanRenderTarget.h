#pragma once

#include <vector>
#include <vulkan/vulkan.h>

class TextureVulkan;

class VulkanRenderTarget 
{
public:
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<TextureVulkan*> colorTextures;
    std::vector<TextureVulkan*> gBufferPos;
    std::vector<TextureVulkan*> gBufferNorm;
    std::vector<TextureVulkan*> gBufferAlbedo;
    std::vector<TextureVulkan*> gPBR;
    std::vector<TextureVulkan*> depthTextures;

    void destroy(VkDevice device) {
        for (size_t i = 0; i < framebuffers.size(); i++) {
            vkDestroyFramebuffer(device, framebuffers[i], nullptr);
        }
        vkDestroyRenderPass(device, renderPass, nullptr);
    }
};