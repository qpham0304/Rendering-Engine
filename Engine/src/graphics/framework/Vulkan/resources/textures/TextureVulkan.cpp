#include "TextureVulkan.h"
#include <core/features/ServiceLocator.h>
#include <graphics/framework/Vulkan/resources/buffers/BufferManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/BufferVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <logging/Logger.h>
#include <core/events/EventManager.h>
#include <vulkan/vulkan.h>
#include <stdexcept>

TextureVulkan::TextureVulkan() :
    textureSampler(VK_NULL_HANDLE),
    textureImageView(VK_NULL_HANDLE),
    textureImage(VK_NULL_HANDLE),
    currentLayout(VK_IMAGE_LAYOUT_UNDEFINED),
    format(VK_FORMAT_UNDEFINED),
    usage(0),
    properties(0),
    aspectBits(VK_IMAGE_ASPECT_NONE),
    tiling(VK_IMAGE_TILING_OPTIMAL)
{
	
}

TextureVulkan::TextureVulkan(uint32_t id) : 
    Texture(id),
    textureSampler(VK_NULL_HANDLE),
    textureImageView(VK_NULL_HANDLE),
    textureImage(VK_NULL_HANDLE),
    currentLayout(VK_IMAGE_LAYOUT_UNDEFINED),
    format(VK_FORMAT_UNDEFINED),
    usage(0),
    properties(0),
    aspectBits(VK_IMAGE_ASPECT_NONE),
    tiling(VK_IMAGE_TILING_OPTIMAL)
{

}

TextureVulkan::~TextureVulkan()
{

}

void TextureVulkan::init(TextureConfig config, TextureSamplerConfig sampler)
{
	BufferManager& bufferManagerTemp = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	bufferManager = &dynamic_cast<BufferManagerVulkan&>(bufferManagerTemp);
	DescriptorManager& descriptorManagerTemp = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManager = &dynamic_cast<DescriptorManagerVulkan&>(descriptorManagerTemp);
	TextureManager& textureManagerTemp = ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
	textureManager = &dynamic_cast<TextureManagerVulkan&>(textureManagerTemp);
    RenderDevice& renderDeviceTemp = ServiceLocator::GetService<RenderDeviceVulkan>("RenderDeviceVulkan");
    renderDevice = &dynamic_cast<RenderDeviceVulkan&>(renderDeviceTemp);

    m_width = config.width;
	m_height = config.height;
	currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	format = config.format;
	usage = config.usage;
	properties = config.properties;
	aspectBits = config.aspectBits;
	tiling = config.tiling;
	mipLevels = config.mipLevels;
	layerCount = config.layerCount;

    samplerInfo = sampler;
}

uint32_t TextureVulkan::getMipLevels()
{
    return mipLevels;
}

uint32_t TextureVulkan::getLayerCount()
{
    return layerCount;
}

void TextureVulkan::transitImage(VkCommandBuffer cmd, VkImageLayout curLayout, VkImageLayout newLayout, int mip, int layer)
{
    TextureManagerVulkan::transitionImageLayout(
        cmd, textureImage, format,
        curLayout, newLayout, mip, layer, renderDevice
    );
    if(curLayout != currentLayout) {
        // Logger& logger = ServiceLocator::GetService<Logger>("Client_LoggerSPD");
        // logger.critical("image transition with mismatch layout {}");
        // throw std::runtime_error("image transition with mismatch");
    }
    currentLayout = newLayout;
}

void TextureVulkan::transitImage(VkCommandBuffer cmd, VkImageLayout curLayout, VkImageLayout newLayout)
{
    transitImage(cmd, currentLayout, newLayout, mipLevels, layerCount);
}

void TextureVulkan::copyFrom(VkCommandBuffer cmd, TextureVulkan* src)
{
    TextureManagerVulkan::copyImage(
        cmd, src, this, 
        format, aspectBits,
        m_width, m_height, renderDevice
    );
}

const VkDescriptorImageInfo &TextureVulkan::getDescImageInfoGeneral()
{
    imageInfoGeneral.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfoGeneral.imageView = textureImageView;
    imageInfoGeneral.sampler = textureSampler;
    
    return imageInfoGeneral;
}

const VkDescriptorImageInfo &TextureVulkan::getDescImageInfoReadOnly()
{
    imageInfoReadOnly.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfoReadOnly.imageView = textureImageView;
    imageInfoReadOnly.sampler = textureSampler;
    
    return imageInfoReadOnly;
}

const TextureSamplerConfig &TextureVulkan::getSamplerInfo()
{
    return samplerInfo;
}

void TextureVulkan::destroy(VkDevice device)
{
	if (textureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, textureSampler, nullptr);
        textureSampler = VK_NULL_HANDLE;
    }
    if (textureImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, textureImageView, nullptr);
        textureImageView = VK_NULL_HANDLE;
    }
    if (textureImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, textureImage, nullptr);
        textureImage = VK_NULL_HANDLE;
    }
    if (textureImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, textureImageMemory, nullptr);
        textureImageMemory = VK_NULL_HANDLE;
    }
}

