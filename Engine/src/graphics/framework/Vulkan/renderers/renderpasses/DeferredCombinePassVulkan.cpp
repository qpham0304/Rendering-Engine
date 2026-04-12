#include "DeferredCombinePassVulkan.h"
#include <core/features/camera.h>
#include <graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <window/AppWindow.h>
#include <math.h>
#include <algorithm>
#include "TemporalPassVulkan.h"

DeferredCombinePassVulkan::DeferredCombinePassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{

}

DeferredCombinePassVulkan::~DeferredCombinePassVulkan()
{

}

bool DeferredCombinePassVulkan::init(WindowConfig config)
{
    PostProcessRendererVulkan::init(config);
        
    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("TemporalPassVulkan");
	auto temporalPassRenderer = dynamic_cast<TemporalPassVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    denoisedGIImage = temporalPassRenderer->getOutputImage();
    sceneImage = deferredRendererVulkan->renderTarget.colorTextures[currentFrame];
    albedoImage = deferredRendererVulkan->renderTarget.gBufferAlbedo[currentFrame];
    

    bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));

    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool DeferredCombinePassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    
    pipeline->destroy();
    
    return true;
}

void DeferredCombinePassVulkan::onUpdate()
{
    PostProcessRendererVulkan::onUpdate();

}

void DeferredCombinePassVulkan::render(Camera &camera)
{
    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("TemporalPassVulkan");
	auto temporalPassRenderer = dynamic_cast<TemporalPassVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    denoisedGIImage = temporalPassRenderer->getOutputImage();
    sceneImage = deferredRendererVulkan->renderTarget.colorTextures[currentFrame];
    albedoImage = deferredRendererVulkan->renderTarget.gBufferAlbedo[currentFrame];

    
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

    ubo.view = camera.getViewMatrix();
    ubo.projection = camera.getProjectionMatrix();
    ubo.projection[1][1] *= -1.0f; 
    ubo.invProj = glm::inverse(ubo.projection);
    ubo.invView = glm::inverse(ubo.view);

	pushConstant.screenRes = { deferredRendererVulkan->renderTarget.width, deferredRendererVulkan->renderTarget.height };
    pushConstant.time = AppWindow::getTime();
    pushConstant.frameSeed = rand() % 32768;

    _updateDescriptor(currentFrame);

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    writeCombinedImage(cmd, currentFrame);

    renderDeviceVulkan->waitIdle();
    rendererManagerVulkan->setDisplayImage(outputImage);
}

void DeferredCombinePassVulkan::writeCombinedImage(VkCommandBuffer cmd, uint32_t currentFrame)
{
    TextureManagerVulkan::transitionImageLayout(
        cmd, outputImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,  1, 1, renderDeviceVulkan);

    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    
    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(setsID)[currentFrame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        pipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

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
        cmd, outputImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);
}

void DeferredCombinePassVulkan::_createResources()
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
            VK_FORMAT_R16G16B16A16_SFLOAT,
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
            VK_FORMAT_R16G16B16A16_SFLOAT,
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
        samplerInfo.maxLod = 1.0f;

        TextureManagerVulkan::createTextureSampler(
            texture->textureSampler, 
            renderDeviceVulkan->device,
            samplerInfo
        );
    };
 
    createTexture(outputImageID, outputImage);

    auto cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
    TextureManagerVulkan::transitionImageLayout(
        cmd, outputImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
    );
    renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

}

void DeferredCombinePassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
    
	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline(
		"assets/shaders/spv/deferredCombine.comp.spv", 
		{ layout }, 
		sizeof(PushConstant)
	);
}

void DeferredCombinePassVulkan::_createDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

	uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 8},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
	};
	
    layoutID = descriptorManagerVulkan->createLayout(bindings);
    poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void DeferredCombinePassVulkan::_updateDescriptor(uint32_t index)
{
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[index]);
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

	VkDescriptorImageInfo outputImageInfo{};
	outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputImageInfo.imageView = outputImage->textureImageView;
	outputImageInfo.sampler = outputImage->textureSampler;

    VkDescriptorImageInfo GIImageInfo{};
	GIImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	GIImageInfo.imageView = denoisedGIImage->textureImageView;
	GIImageInfo.sampler = denoisedGIImage->textureSampler;

    VkDescriptorImageInfo sceneColorImageInfo{};
	sceneColorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	sceneColorImageInfo.imageView = sceneImage->textureImageView;
	sceneColorImageInfo.sampler = sceneImage->textureSampler;

    VkDescriptorImageInfo albedoImageInfo{};
	albedoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	albedoImageInfo.imageView = albedoImage->textureImageView;
	albedoImageInfo.sampler = albedoImage->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeUniform(&writes, descriptorSets[index], 0, bufferInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 1, outputImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 2, GIImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 3, sceneColorImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 4, albedoImageInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void DeferredCombinePassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();
    _createResources();
	_createDescriptors();
	_createPipelines();
}

void DeferredCombinePassVulkan::_cleanupResources()
{
    textureManagerVulkan->destroy(outputImage->id());
    pipeline->destroy();
}
