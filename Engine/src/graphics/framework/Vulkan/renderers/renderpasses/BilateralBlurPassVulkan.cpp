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

}

BilateralBlurPassVulkan::~BilateralBlurPassVulkan() 
{

}

bool BilateralBlurPassVulkan::init(WindowConfig config) 
{
    RendererVulkan::init(config);
    
    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    positionImage = deferredRendererVulkan->renderTarget.gBufferPos[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];
    
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
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    positionImage = deferredRendererVulkan->renderTarget.gBufferPos[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];


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
    aoMapID = textureManagerVulkan->createTexture();
    aoMap = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(aoMapID));
	
    assert(aoMap && "failed to cast texture into vulkan texture");
	
    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;
    
    TextureManagerVulkan::createImage(
        swapchain.swapChainExtent.width,
        swapchain.swapChainExtent.height,
        VK_FORMAT_R16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        aoMap->textureImage,
        aoMap->textureImageMemory,
        1,
        renderDeviceVulkan->device
    );

    TextureManagerVulkan::createImageView(
        aoMap->textureImage,
        aoMap->textureImageView,
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
        aoMap->textureSampler, 
        renderDeviceVulkan->device,
        samplerInfo
    );
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