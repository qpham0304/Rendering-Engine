#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"
#include "graphics/framework/Vulkan/resources/textures/TextureVulkan.h"

class VulkanRenderTarget 
{
public:
    uint32_t width;
    uint32_t height;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<TextureVulkan*> colorTextures;
    std::vector<TextureVulkan*> gBufferPos;
    std::vector<TextureVulkan*> gBufferNorm;
    std::vector<TextureVulkan*> gBufferAlbedo;
    std::vector<TextureVulkan*> gPBR;
    std::vector<TextureVulkan*> gBufferEmissive;
    std::vector<TextureVulkan*> gBufferMotion;
    std::vector<TextureVulkan*> depthTextures;

    void destroy(VkDevice device) {
        for (size_t i = 0; i < framebuffers.size(); i++) {
            vkDestroyFramebuffer(device, framebuffers[i], nullptr);
        }
        vkDestroyRenderPass(device, renderPass, nullptr);

        TextureManager& textureManager = ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
        for(TextureVulkan* texture : colorTextures){
            textureManager.destroy(texture->id());
        }
        for(auto& texture : gBufferPos){
            textureManager.destroy(texture->id());
        }
        for(auto& texture : gBufferNorm){
            textureManager.destroy(texture->id());
        }
        for(auto& texture : gBufferAlbedo){
            textureManager.destroy(texture->id());
        }
        for(auto& texture : gPBR){
            textureManager.destroy(texture->id());
        }
        for(auto& texture : gBufferEmissive){
            textureManager.destroy(texture->id());
        }
        for(auto& texture : gBufferMotion){
            textureManager.destroy(texture->id());
        }
        for(auto& texture : depthTextures){
            textureManager.destroy(texture->id());
        }
    }
};