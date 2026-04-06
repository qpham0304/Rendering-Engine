#include "PostProcessRendererVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/camera.h>
#include <graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>

PostProcessRendererVulkan::PostProcessRendererVulkan(std::string serviceName)
	:	RendererVulkan(serviceName)
{

}

PostProcessRendererVulkan::~PostProcessRendererVulkan() 
{

}

bool PostProcessRendererVulkan::init(WindowConfig config) 
{
    RendererVulkan::init(config);
    
    createImage();
    createDescriptors();
	createPipelines();
    
    return true;
}
bool PostProcessRendererVulkan::onClose() 
{
    RendererVulkan::onClose();

    cleanupResources();

    return true;
}
void PostProcessRendererVulkan::onUpdate() 
{
    RendererVulkan::onUpdate();

}

void PostProcessRendererVulkan::render(Camera& camera) 
{
    RendererVulkan::render(camera);
    
}

TextureVulkan *PostProcessRendererVulkan::getOutputImage()
{
    return outputImage;
}

void PostProcessRendererVulkan::createImage()
{
    auto createTexture = [this] (uint32_t& id, TextureVulkan*& texture){
        id = textureManagerVulkan->createTexture();
        texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
        
        assert(texture && "failed to cast texture into vulkan texture");
        
        VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
        VkDevice device = renderDeviceVulkan->device;
        
        TextureManagerVulkan::createImage(
            swapchain.swapChainExtent.width,
            swapchain.swapChainExtent.height,
            VK_FORMAT_R16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture->textureImage,
            texture->textureImageMemory,
            1,
            renderDeviceVulkan->device
        );

        TextureManagerVulkan::createImageView(
            texture->textureImage,
            texture->textureImageView,
            VK_FORMAT_R16_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1,
            renderDeviceVulkan->device
        );

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        TextureManagerVulkan::createTextureSampler(
            texture->textureSampler, 
            renderDeviceVulkan->device,
            samplerInfo
        );
    };
 
    createTexture(outputImageID, outputImage);
}

void PostProcessRendererVulkan::createPipelines()
{
	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);

    
}

void PostProcessRendererVulkan::createDescriptors()
{
    layoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	}, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT);

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 3},
	};
	
	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);

	for(int i = 0; i < frameCount; i++) {
        updateDescriptor(i);
    }

}

void PostProcessRendererVulkan::updateDescriptor(uint32_t index)
{
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);
	VkDescriptorImageInfo aoImageInfo{};
	aoImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	aoImageInfo.imageView = outputImage->textureImageView;
	aoImageInfo.sampler = outputImage->textureSampler;
    
	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 0, aoImageInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void PostProcessRendererVulkan::recreateResources()
{
    renderDeviceVulkan->waitIdle();
    cleanupResources();
    createImage();
    createDescriptors();
    createPipelines();
}

void PostProcessRendererVulkan::cleanupResources()
{
    textureManagerVulkan->destroy(outputImage->id());
    pipeline->destroy();
}
