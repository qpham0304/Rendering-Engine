#include "RenderGraphVulkan.h"
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>

RenderGraphVulkan::RenderGraphVulkan()
{
    
}

void RenderGraphVulkan::addPass(RenderPassNode node)
{
    m_passes.push_back(std::move(node));
}

void RenderGraphVulkan::execute(VkCommandBuffer cmd, uint32_t currentFrame)
{
    //TODO: use sorted pass eventually
    for(auto& passNode : m_passes) {
        for(auto& access : passNode.reads) {
            RGResource& resource = m_resources[access.handle.index];
            if(resource.initialLayout != access.layout) {
                resource.texture->transitImage(cmd, resource.initialLayout, access.layout);
                resource.initialLayout = access.layout;
            }
        }
        
        for(auto& access : passNode.writes) {
            RGResource& resource = m_resources[access.handle.index];
            if(resource.initialLayout != access.layout) {
                resource.texture->transitImage(cmd, resource.initialLayout, access.layout);
                resource.initialLayout = access.layout;
            }
        }

        passNode.execute(cmd, currentFrame);
    }

    for(auto& resource : m_resources) {
        if(resource.finalLayout != VK_IMAGE_LAYOUT_UNDEFINED && resource.initialLayout != resource.finalLayout) {
            resource.texture->transitImage(cmd, resource.initialLayout, resource.finalLayout);
            resource.initialLayout = resource.finalLayout;
        }
    }
}

void RenderGraphVulkan::reset()
{
    m_resources.clear();
    m_passes.clear();
}

RGHandle RenderGraphVulkan::importTexture(TextureVulkan* texture, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
    //find matchin imported texture
    for(uint32_t i = 0; i < m_resources.size(); i++) {
        if(m_resources[i].texture == texture) {
            return { i };
        }
    }
    uint32_t size = m_resources.size();

    RGResource resource{};
    resource.texture = texture;
    resource.initialLayout = initialLayout;
    resource.finalLayout = finalLayout;
    
    m_resources.push_back(resource);

    return { size };
}
