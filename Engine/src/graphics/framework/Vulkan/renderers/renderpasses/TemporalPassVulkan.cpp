#include "TemporalPassVulkan.h"
#include <core/features/camera.h>
#include <graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/SSRGIPassVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <window/AppWindow.h>

TemporalPassVulkan::TemporalPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{

}

TemporalPassVulkan::~TemporalPassVulkan()
{

}

bool TemporalPassVulkan::init(WindowConfig config)
{
    PostProcessRendererVulkan::init(config);
        
    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("SSRGIPassVulkan");
	auto SSRGIPassRenderer = dynamic_cast<SSRGIPassVulkan*>(renderer);

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    ssrgiImage = SSRGIPassRenderer->getOutputImage();
    motionImage = deferredRendererVulkan->renderTarget.gBufferMotion[currentFrame];
    depthImage = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];

    bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));

    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool TemporalPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    
    pipeline->destroy();
    
    return true;
}

void TemporalPassVulkan::onUpdate()
{
    PostProcessRendererVulkan::onUpdate();

}

void TemporalPassVulkan::render(Camera &camera)
{
    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("SSRGIPassVulkan");
	auto SSRGIPassRenderer = dynamic_cast<SSRGIPassVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    ssrgiImage = SSRGIPassRenderer->getOutputImage();
    motionImage = deferredRendererVulkan->renderTarget.gBufferMotion[currentFrame];
    depthImage = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];

    
    _updateDescriptor(TemporalSetsID, currentFrame, outputImage, ssrgiHistoryImage);
    _updateDescriptor(TemporalSetsHistoryID, currentFrame, ssrgiHistoryImage, outputImage);
    
    glm::mat4 currentView = camera.getViewMatrix();
    glm::mat4 currentProj = camera.getProjectionMatrix();
    currentProj[1][1] *= -1;
    glm::mat4 currentViewProj = currentProj * currentView;
    ubo.invViewProj = glm::inverse(currentViewProj);
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	pushConstant.screenRes = { deferredRendererVulkan->renderTarget.width, deferredRendererVulkan->renderTarget.height };
    pushConstant.maxAccumulation = 128.0f;

    writeTP(renderDeviceVulkan->commandPool.currentBuffer(), currentFrame);

    ubo.priorViewProj = currentViewProj;

    renderDeviceVulkan->waitIdle();
    rendererManagerVulkan->setDisplayImage(outputImage);
}

void TemporalPassVulkan::writeTP(VkCommandBuffer cmd, uint32_t currentFrame)
{
    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
        
    flip = !flip;
    TextureVulkan* currentWriteTarget = flip ? outputImage : ssrgiHistoryImage;
    TextureVulkan* currentReadHistory = flip ? ssrgiHistoryImage : outputImage;

    TextureManagerVulkan::transitionImageLayout(
        cmd, currentWriteTarget->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan);

    uint32_t currentSets = flip ? TemporalSetsID : TemporalSetsHistoryID;
    auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(currentSets);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        pipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 
        0, sizeof(PushConstant), &pushConstant);

    auto renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    uint32_t width = deferredRendererVulkan->renderTarget.width;
    uint32_t height = deferredRendererVulkan->renderTarget.height;
    uint32_t groupX = (width  + 15) / 16;
    uint32_t groupY = (height + 15) / 16;

    vkCmdDispatch(cmd, groupX, groupY, 1);
    
    TextureManagerVulkan::transitionImageLayout(
        cmd, currentWriteTarget->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    _copyDepthToHistory(cmd);
}

void TemporalPassVulkan::writeTAA(VkCommandBuffer cmd, uint32_t currentFrame)
{
    
}

void TemporalPassVulkan::_createResources()
{
    auto createTexture = [this] (
        uint32_t& id, 
        TextureVulkan*& texture, 
        VkFormat format, 
        VkImageAspectFlagBits aspect, 
        VkImageUsageFlags extraUsage
    ){
        id = textureManagerVulkan->createTexture();
        texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
        
        assert(texture && "failed to cast texture into vulkan texture");
        
        VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
        VkDevice device = renderDeviceVulkan->device;
        
        TextureManagerVulkan::createImage(
            swapchain.swapChainExtent.width,
            swapchain.swapChainExtent.height,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | extraUsage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture->textureImage,
            texture->textureImageMemory,
            1,
            renderDeviceVulkan->device
        );

        TextureManagerVulkan::createImageView(
            texture->textureImage,
            texture->textureImageView,
            format,
            aspect,
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
        samplerInfo.maxLod = 11.0f;

        TextureManagerVulkan::createTextureSampler(
            texture->textureSampler, 
            renderDeviceVulkan->device,
            samplerInfo
        );
    };
 
    VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
    createTexture(outputImageID, outputImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0);
    createTexture(ssrgiHistoryImageID, ssrgiHistoryImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0);
    createTexture(prevDepthImageID, prevDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    createTexture(prevNormalImageID, prevNormalImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    VkCommandBuffer tempCmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

    TextureManagerVulkan::transitionImageLayout(tempCmd, outputImage->textureImage, 
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(tempCmd, ssrgiHistoryImage->textureImage, 
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(tempCmd, prevDepthImage->textureImage, 
        depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(tempCmd, prevNormalImage->textureImage, 
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    renderDeviceVulkan->commandPool.endSingleTimeCommand(tempCmd);
}

void TemporalPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(TemporalLayoutID);
    
	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline(
		"assets/shaders/spv/temporalProjection.comp.spv", 
		{ layout }, 
		sizeof(PushConstant)
	);
}

void TemporalPassVulkan::_createDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings {
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

	uint32_t frameCount = VulkanUtils::numFrames();
    uint32_t totalSetsNeeded = frameCount * 2;
    std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1 * totalSetsNeeded},
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 7  * totalSetsNeeded},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1  * totalSetsNeeded},
	};
	
    TemporalLayoutID = descriptorManagerVulkan->createLayout(bindings);
    TemporalPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount * totalSetsNeeded);
	TemporalSetsID = descriptorManagerVulkan->createSets(TemporalLayoutID, TemporalPoolID, frameCount);
	TemporalSetsHistoryID = descriptorManagerVulkan->createSets(TemporalLayoutID, TemporalPoolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(TemporalSetsID, i, outputImage, ssrgiHistoryImage);
        _updateDescriptor(TemporalSetsHistoryID, i, ssrgiHistoryImage, outputImage);
    }
}

void TemporalPassVulkan::_updateDescriptor(uint32_t currentSetsID, uint32_t index, TextureVulkan* writeImg, TextureVulkan* readImage)
{
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(currentSetsID);
    
    VkDescriptorImageInfo outputImageInfo{};
	outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputImageInfo.imageView = writeImg->textureImageView;
	outputImageInfo.sampler = writeImg->textureSampler;

    VkDescriptorImageInfo ssrgiImageInfo{};
	ssrgiImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ssrgiImageInfo.imageView = ssrgiImage->textureImageView;
	ssrgiImageInfo.sampler = ssrgiImage->textureSampler;

    VkDescriptorImageInfo ssrgiHistoryImageInfo{};
	ssrgiHistoryImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ssrgiHistoryImageInfo.imageView = readImage->textureImageView;
	ssrgiHistoryImageInfo.sampler = readImage->textureSampler;
    
    VkDescriptorImageInfo motionImageInfo{};
	motionImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	motionImageInfo.imageView = motionImage->textureImageView;
	motionImageInfo.sampler = motionImage->textureSampler;

    VkDescriptorImageInfo currDepthImageInfo{};
	currDepthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	currDepthImageInfo.imageView = depthImage->textureImageView;
	currDepthImageInfo.sampler = depthImage->textureSampler;

    VkDescriptorImageInfo prevDepthImageInfo{};
	prevDepthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	prevDepthImageInfo.imageView = prevDepthImage->textureImageView;
	prevDepthImageInfo.sampler = prevDepthImage->textureSampler;

    VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[index]);
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo currNormalImageInfo{};
	currNormalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	currNormalImageInfo.imageView = normalImage->textureImageView;
	currNormalImageInfo.sampler = normalImage->textureSampler;

    VkDescriptorImageInfo prevNormalImageInfo{};
	prevNormalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	prevNormalImageInfo.imageView = prevNormalImage->textureImageView;
	prevNormalImageInfo.sampler = prevNormalImage->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 0, outputImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 1, ssrgiImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 2, ssrgiHistoryImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 3, motionImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 4, currDepthImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 5, prevDepthImageInfo);
	descriptorManagerVulkan->writeUniform(&writes, descriptorSets[index], 6, bufferInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 7, currNormalImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 8, prevNormalImageInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void TemporalPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();
    _createResources();
	_createDescriptors();
	_createPipelines();
}

void TemporalPassVulkan::_cleanupResources()
{
    textureManagerVulkan->destroy(outputImage->id());
    pipeline->destroy();
}

void TemporalPassVulkan::_copyDepthToHistory(VkCommandBuffer cmd) {
    TextureManagerVulkan::copyImage(cmd, depthImage, prevDepthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, renderDeviceVulkan);
    TextureManagerVulkan::copyImage(cmd, normalImage, prevNormalImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, renderDeviceVulkan);
}