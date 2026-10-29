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
    setFootprint("PostProcessRendererVulkan", sizeof(PostProcessRendererVulkan), MemorySystem::Graphics);

}

PostProcessRendererVulkan::~PostProcessRendererVulkan() 
{

}

bool PostProcessRendererVulkan::init(WindowConfig config) 
{
    RendererVulkan::init(config);
    
    // createResources();
    // createDescriptors();
	// createPipelines();
    
    return true;
}
bool PostProcessRendererVulkan::onClose() 
{
    RendererVulkan::onClose();

    // cleanupResources();  empty pipeline cleanup causes issue atm

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

void PostProcessRendererVulkan::createResources()
{
    auto createTexture = [this] (uint32_t& id, TextureVulkan*& texture){
        VkExtent2D extent = renderDeviceVulkan->swapchain.swapChainExtent;
        //TODO: remove general creation or change this to RGBA16FLOAT
        TextureConfig imageConfig {
            .width = extent.width,
            .height = extent.height,
            .format = VK_FORMAT_R16_SFLOAT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        id = textureManagerVulkan->createTexture(imageConfig, TextureSamplerConfig{});
        texture = textureManagerVulkan->getTexture(id);
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
    createResources();
    createDescriptors();
    createPipelines();
}

void PostProcessRendererVulkan::cleanupResources()
{
    textureManagerVulkan->destroy(outputImage->id());
    pipeline->destroy();
}
