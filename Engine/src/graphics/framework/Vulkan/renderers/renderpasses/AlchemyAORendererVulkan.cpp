#include "AlchemyAORendererVulkan.h"
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

AlchemyAORendererVulkan::AlchemyAORendererVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{

}

AlchemyAORendererVulkan::~AlchemyAORendererVulkan() 
{

}

bool AlchemyAORendererVulkan::init(WindowConfig config) 
{
    PostProcessRendererVulkan::init(config);
    
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
    
    pushConstant.radius = 1.0;
    pushConstant.bias = 0.001;
    pushConstant.intensity = 1.0;
    
    blurrPushConstant.blurRadius = 4;
    blurrPushConstant.scale = 100.0f;

    return true;
}
bool AlchemyAORendererVulkan::onClose() 
{
    PostProcessRendererVulkan::onClose();

    _cleanupResources();

    return true;
}
void AlchemyAORendererVulkan::onUpdate() 
{
    PostProcessRendererVulkan::onUpdate();

}

void AlchemyAORendererVulkan::render(Camera& camera) 
{
    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

	RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    positionImage = deferredRendererVulkan->renderTarget.gBufferPos[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];


	pushConstant.view = camera.getViewMatrix();
    pushConstant.projScale = camera.getViewHeight() / (2.0 * tan(camera.getFOV() / 2));
    
    float fovRadians = glm::radians(camera.getFOV()); 
    pushConstant.projScale = static_cast<float>(camera.getViewHeight()) / (2.0f * tanf(fovRadians * 0.5f));


	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    writeAO(cmd, currentFrame);
    writeBlur(cmd, currentFrame);
}

void AlchemyAORendererVulkan::writeAO(VkCommandBuffer cmd, uint32_t currentFrame)
{
    TextureManagerVulkan::transitionImageLayout(
        cmd, aoMap->textureImage, VK_FORMAT_R16_SFLOAT, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,  1, 1, renderDeviceVulkan);
    
    TextureManagerVulkan::transitionImageLayout(
        cmd, positionImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan);
    
    TextureManagerVulkan::transitionImageLayout(
        cmd, normalImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan);

    _updateDescriptorSetsAO(currentFrame);
    
    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
    occlusionPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    vkCmdPushConstants(
        cmd, 
        occlusionPipeline->pipelineLayout, 
        VK_SHADER_STAGE_COMPUTE_BIT, 
        0, 
        sizeof(pushConstant), 
        &pushConstant
    );

    auto sets = descriptorManagerVulkan->getDescriptorSet(occlusionDecriptorsetsID);
    vkCmdBindDescriptorSets(
        cmd, 
        VK_PIPELINE_BIND_POINT_COMPUTE, 
        occlusionPipeline->pipelineLayout, 
        0, 
        1, 
        &sets[currentFrame], 
        0, nullptr
    );

	RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    uint32_t groupX = (deferredRendererVulkan->renderTarget.width + 15) / 16;
    uint32_t groupY = (deferredRendererVulkan->renderTarget.height + 15) / 16;
	vkCmdDispatch(cmd, groupX, groupY, 1);

    TextureManagerVulkan::transitionImageLayout(
        cmd, positionImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);
    
    TextureManagerVulkan::transitionImageLayout(
        cmd, normalImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT, 
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(cmd, aoMap->textureImage, VK_FORMAT_R16_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
    );
}

void AlchemyAORendererVulkan::writeBlur(VkCommandBuffer cmd, uint32_t currentFrame)
{
    TextureManagerVulkan::transitionImageLayout(cmd, aoMap->textureImage, VK_FORMAT_R16_SFLOAT, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan);
    
    TextureManagerVulkan::transitionImageLayout(cmd, aoMapTemp->textureImage, VK_FORMAT_R16_SFLOAT, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(cmd, positionImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan);

    _updateDescriptorSetsBlur(currentFrame);

    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
    uint32_t groupX = (swapchain.swapChainExtent.width + 15) / 16;
    uint32_t groupY = (swapchain.swapChainExtent.height + 15) / 16;

    blurPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    blurrPushConstant.isVertical = 0.0f;
    vkCmdPushConstants(cmd, blurPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BlurPushConstantInfo), &blurrPushConstant);
    auto mToT_Sets = descriptorManagerVulkan->getDescriptorSet(blurDescSetMtoT_ID);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipeline->pipelineLayout, 0, 1, &mToT_Sets[currentFrame], 0, nullptr);
    vkCmdDispatch(cmd, groupX, groupY, 1);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.image = aoMapTemp->textureImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    blurrPushConstant.isVertical = 1.0f;
    vkCmdPushConstants(cmd, blurPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BlurPushConstantInfo), &blurrPushConstant);
    auto tToM_Sets = descriptorManagerVulkan->getDescriptorSet(blurDescSetTtoM_ID);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipeline->pipelineLayout, 0, 1, &tToM_Sets[currentFrame], 0, nullptr);
    vkCmdDispatch(cmd, groupX, groupY, 1);

    TextureManagerVulkan::transitionImageLayout(cmd, aoMap->textureImage, VK_FORMAT_R16_SFLOAT, 
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);
    TextureManagerVulkan::transitionImageLayout(cmd, positionImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT, 
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);
}

void AlchemyAORendererVulkan::_createOcclusionMap()
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
 
    createTexture(aoMapID, aoMap);
    createTexture(aoMapTempID, aoMapTemp);
    outputImage = aoMap;
}

void AlchemyAORendererVulkan::_createDescriptors()
{
    occlusionDescriptorLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
	}, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT);

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 3},
	};
	
	occlusionDescriptorPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
	occlusionDecriptorsetsID = descriptorManagerVulkan->createSets(occlusionDescriptorLayoutID, occlusionDescriptorPoolID, frameCount);

	for(int i = 0; i < frameCount; i++) {
        _updateDescriptorSetsAO(i);
    }


    blurDescriptorLayoutID = descriptorManagerVulkan->createLayout({
        { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }, // Input
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }, // Depth (for bilateral)
        { 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }  // Output
    });

    std::vector<VkDescriptorPoolSize> blurPoolSizes {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 6}, // 3 for each pass
    };

    blurDescriptorPoolID = descriptorManagerVulkan->createPool(blurPoolSizes, frameCount * 2);

    // Create two sets per frame for the ping-pong
    blurDescSetMtoT_ID = descriptorManagerVulkan->createSets(blurDescriptorLayoutID, blurDescriptorPoolID, frameCount);
    blurDescSetTtoM_ID = descriptorManagerVulkan->createSets(blurDescriptorLayoutID, blurDescriptorPoolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptorSetsBlur(i);
    }
}

void AlchemyAORendererVulkan::_createPipelines()
{
    VkDescriptorSetLayout descriptorLayoutLUT = descriptorManagerVulkan->getDescriptorLayout(occlusionDescriptorLayoutID);
	occlusionPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	occlusionPipeline->createComputePipeline(
		"assets/shaders/spv/alchemyAO.comp.spv", 
		{ descriptorLayoutLUT }, 
		sizeof(PushConstantInfo)
	);

    VkDescriptorSetLayout blurDescriptorLayout = descriptorManagerVulkan->getDescriptorLayout(blurDescriptorLayoutID);
	blurPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	blurPipeline->createComputePipeline(
		"assets/shaders/spv/bilateralBlur.comp.spv", 
		{ blurDescriptorLayout }, 
		sizeof(PushConstantInfo)
	);
}

void AlchemyAORendererVulkan::_updateDescriptorSetsAO(uint32_t index)
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

void AlchemyAORendererVulkan::_updateDescriptorSetsBlur(uint32_t index)
{
    auto mToT_Sets = descriptorManagerVulkan->getDescriptorSet(blurDescSetMtoT_ID);
    auto tToM_Sets = descriptorManagerVulkan->getDescriptorSet(blurDescSetTtoM_ID);

    // layout MUST match what the image is in during vkCmdDispatch
    VkImageLayout computeLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo posInfo{ positionImage->textureSampler, positionImage->textureImageView, computeLayout };
    VkDescriptorImageInfo aoInputInfo{ aoMap->textureSampler, aoMap->textureImageView, computeLayout };
    VkDescriptorImageInfo tempInfo{ aoMapTemp->textureSampler, aoMapTemp->textureImageView, computeLayout };

    std::vector<VkWriteDescriptorSet> writes;
    
    descriptorManagerVulkan->writeStorageImage(&writes, mToT_Sets[index], 0, aoInputInfo);
    descriptorManagerVulkan->writeStorageImage(&writes, mToT_Sets[index], 1, posInfo);
    descriptorManagerVulkan->writeStorageImage(&writes, mToT_Sets[index], 2, tempInfo);

    descriptorManagerVulkan->writeStorageImage(&writes, tToM_Sets[index], 0, tempInfo);
    descriptorManagerVulkan->writeStorageImage(&writes, tToM_Sets[index], 1, posInfo);
    descriptorManagerVulkan->writeStorageImage(&writes, tToM_Sets[index], 2, aoInputInfo);

    descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void AlchemyAORendererVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();
    _createOcclusionMap();
    _createDescriptors();
	_createPipelines();
}

void AlchemyAORendererVulkan::_cleanupResources()
{
    textureManagerVulkan->destroy(aoMap->id());
    textureManagerVulkan->destroy(aoMapTemp->id());
    occlusionPipeline->destroy();
    blurPipeline->destroy();

}