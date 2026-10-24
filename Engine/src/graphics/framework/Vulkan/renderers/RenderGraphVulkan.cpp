#include "RenderGraphVulkan.h"
#include <core/features/ServiceLocator.h>
#include <graphics/renderers/RenderDevice.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>

RenderGraphVulkan::RenderGraphVulkan()
{
    setFootprint("RenderGraphVulkan", sizeof(RenderGraphVulkan), MemorySystem::Graphics);
    
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
        
        TextureManagerVulkan::createBarrier(    // same layout, only makes the previous accesses finish and their writes visible
            cmd, resource.texture->textureImage,
            VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            access.layout, access.layout,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_REMAINING_MIP_LEVELS, VK_REMAINING_ARRAY_LAYERS, resource.texture->format	// old path textures (ibl cubemap) do not record their mips and layers
        );
    }

    resource.lastWasWrite = isWrite;
}

void RenderGraphVulkan::setRenderSize(uint32_t width, uint32_t height)
{
    m_renderWidth = width;
    m_renderHeight = height;
}

RGHandle RenderGraphVulkan::createRenderTexture(const std::string& name, const RGTextureDesc& desc)
{
    return importTexture(_createTexture(name, desc));
}

TextureVulkan* RenderGraphVulkan::getTexture(RGHandle handle)
{
    return m_resources[handle.index].texture;
}

// the render graph authors texture and handle recreation every frame
TextureVulkan* RenderGraphVulkan::_createTexture(const std::string& name, const RGTextureDesc& desc)
{
    bool fixedSize = desc.config.width > 0 && desc.config.height > 0;
    uint32_t width = fixedSize ? desc.config.width : std::max(1u, static_cast<uint32_t>(m_renderWidth * desc.scale));
    uint32_t height = fixedSize ? desc.config.height : std::max(1u, static_cast<uint32_t>(m_renderHeight * desc.scale));

    auto& textureManager = dynamic_cast<TextureManagerVulkan&>(ServiceLocator::GetService<TextureManager>("TextureManagerVulkan"));

    auto it = m_ownedTextures.find(name);
    if(it != m_ownedTextures.end()) {
        RenderTexture& owned = it->second;
        const TextureConfig& a = owned.desc.config;
        const TextureConfig& b = desc.config;
        bool sameConfig = a.format == b.format && a.usage == b.usage && a.mipLevels == b.mipLevels
            && a.layerCount == b.layerCount && a.aspectBits == b.aspectBits && a.initialLayout == b.initialLayout;
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

    // the contents are discarded every frame, the first pass has to overwrite the whole image
    if(texture->getInitialLayout() == VK_IMAGE_LAYOUT_UNDEFINED) {
        initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    RGResource resource{};
    resource.texture = texture;
    resource.initialLayout = initialLayout;
    resource.finalLayout = finalLayout;
    
    m_resources.push_back(resource);

    return { size };
}
