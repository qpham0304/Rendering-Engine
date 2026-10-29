#include "BilateralBlurPassVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/camera.h>
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>

BilateralBlurPassVulkan::BilateralBlurPassVulkan(std::string serviceName)
	:	RendererVulkan(serviceName)
{
	setFootprint("BilateralBlurPassVulkan", sizeof(BilateralBlurPassVulkan), MemorySystem::Graphics);

}

BilateralBlurPassVulkan::~BilateralBlurPassVulkan() 
{

}

bool BilateralBlurPassVulkan::init(WindowConfig config) 
{
    RendererVulkan::init(config);
    
    RendererVulkan* renderer = nullptr;
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = geometryPass->renderTarget.depthTexture;
    positionImage = geometryPass->renderTarget.gBufferPos;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    
    _createOcclusionMap();
    _createDescriptors();
	_createPipelines();

    return true;
}
bool BilateralBlurPassVulkan::onClose() 
{
    RendererVulkan::onClose();

    _cleanupResources();

    return true;
}
void BilateralBlurPassVulkan::onUpdate() 
{

}

void BilateralBlurPassVulkan::render(Camera& camera) 
{
	RendererVulkan* renderer = nullptr;
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = geometryPass->renderTarget.depthTexture;
    positionImage = geometryPass->renderTarget.gBufferPos;
    normalImage = geometryPass->renderTarget.gBufferNorm;


	pushConstant.view = camera.getViewMatrix();
    pushConstant.radius = 1.0;
    pushConstant.bias = 0.001;
    pushConstant.intensity = 1.0;

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    
}


void BilateralBlurPassVulkan::writeBlur()
{

}

void BilateralBlurPassVulkan::_createOcclusionMap()
{
    VkExtent2D extent = renderDeviceVulkan->swapchain.swapChainExtent;
    TextureConfig imageConfig {
        .width = extent.width,
        .height = extent.height,
        .format = VK_FORMAT_R16_SFLOAT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    aoMapID = textureManagerVulkan->createTexture(imageConfig, TextureSamplerConfig{});
    aoMap = textureManagerVulkan->getTexture(aoMapID);
}

void BilateralBlurPassVulkan::_createDescriptors()
{
    occlusionDescriptorLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 3},
	};
	
	occlusionDescriptorPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);

	occlusionDecriptorsetsID = descriptorManagerVulkan->createSets(occlusionDescriptorLayoutID, occlusionDescriptorPoolID, frameCount);

	for(int i = 0; i < frameCount; i++) {
        _updateDescriptorSets(i);
    }
}

void BilateralBlurPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout descriptorLayoutLUT = descriptorManagerVulkan->getDescriptorLayout(occlusionDescriptorLayoutID);
	occlusionPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	occlusionPipeline->createComputePipeline(
		"assets/shaders/spv/alchemyAO.comp.spv", 
		{ descriptorLayoutLUT }, 
		sizeof(PushConstantInfo)
	);
}

void BilateralBlurPassVulkan::_updateDescriptorSets(uint32_t index)
{
	auto lutDescriptorSets = descriptorManagerVulkan->getDescriptorSet(occlusionDecriptorsetsID);
	VkDescriptorImageInfo aoImageInfo{};
	aoImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	aoImageInfo.imageView = aoMap->textureImageView;
	aoImageInfo.sampler = aoMap->textureSampler;

    VkDescriptorImageInfo positionImageInfo{};
	positionImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	positionImageInfo.imageView = positionImage->textureImageView;
	positionImageInfo.sampler = positionImage->textureSampler;

    VkDescriptorImageInfo normalImageInfo{};
	normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	normalImageInfo.imageView = normalImage->textureImageView;
	normalImageInfo.sampler = normalImage->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, lutDescriptorSets[index], 0, aoImageInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, lutDescriptorSets[index], 1, positionImageInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, lutDescriptorSets[index], 2, normalImageInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void BilateralBlurPassVulkan::_recreateResources()
{
    
}

void BilateralBlurPassVulkan::_cleanupResources()
{
    occlusionPipeline->destroy();
}

void BilateralBlurPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
}
