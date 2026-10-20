#include "RenderGraphVulkan.h"
#include <core/features/ServiceLocator.h>
#include <graphics/renderers/RenderDevice.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
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
            _prepareAccess(cmd, access, false);
        }

        for(auto& access : passNode.writes) {
            _prepareAccess(cmd, access, true);
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

// gets the texture into the layout this access needs. a layout change carries its own barrier,
// when the layout stays the same a barrier is still needed if a write is on either side (only read after read is free)
void RenderGraphVulkan::_prepareAccess(VkCommandBuffer cmd, const RGAccess& access, bool isWrite)
{
    RGResource& resource = m_resources[access.handle.index];

    if(resource.initialLayout != access.layout) {
        resource.texture->transitImage(cmd, resource.initialLayout, access.layout);
        resource.initialLayout = access.layout;
    } else if(resource.lastWasWrite || isWrite) {
        _memoryBarrier(cmd, resource.texture, access.layout);
    }

    resource.lastWasWrite = isWrite;
}

// same layout on both sides, only makes the previous accesses finish and their writes visible.
// all commands to all commands is broad but always correct, it can be narrowed per usage later
void RenderGraphVulkan::_memoryBarrier(VkCommandBuffer cmd, TextureVulkan* texture, VkImageLayout layout)
{
    VkFormat format = texture->format;
    bool hasStencil = format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT;
    bool isDepth = hasStencil || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM;

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if(isDepth) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = layout;
    barrier.newLayout = layout;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture->textureImage;
    //TODO: every mip and layer, some textures like bloom, hiz never record their real mip count on the TextureVulkan
    // add add a way to support that on texture vulkan eventually
    barrier.subresourceRange = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );
}

void RenderGraphVulkan::setRenderSize(uint32_t width, uint32_t height)
{
    m_renderWidth = width;
    m_renderHeight = height;
}

// the render graph authors texture and handle recreation every frame
TextureVulkan* RenderGraphVulkan::acquireTexture(const std::string& name, const RGTextureDesc& desc)
{
    uint32_t width = std::max(1u, static_cast<uint32_t>(m_renderWidth * desc.scale));
    uint32_t height = std::max(1u, static_cast<uint32_t>(m_renderHeight * desc.scale));

    auto& textureManager = dynamic_cast<TextureManagerVulkan&>(ServiceLocator::GetService<TextureManager>("TextureManagerVulkan"));

    auto it = m_ownedTextures.find(name);
    if(it != m_ownedTextures.end()) {
        RGOwnedTexture& owned = it->second;
        const TextureConfig& a = owned.desc.config;
        const TextureConfig& b = desc.config;
        bool sameConfig = a.format == b.format && a.usage == b.usage && a.mipLevels == b.mipLevels
            && a.layerCount == b.layerCount && a.aspectBits == b.aspectBits;
        if(owned.width == width && owned.height == height && sameConfig) {
            return owned.texture;
        }

        auto& renderDevice = dynamic_cast<RenderDeviceVulkan&>(ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan"));
        renderDevice.waitIdle();
        textureManager.destroy(owned.texture->id());
        m_ownedTextures.erase(it);
    }

    TextureConfig config = desc.config;
    config.width = width;
    config.height = height;

    uint32_t id = textureManager.createTexture(config, desc.sampler);
    TextureVulkan* texture = textureManager.getTexture(id);

    m_ownedTextures[name] = { desc, width, height, texture };
    return texture;
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
