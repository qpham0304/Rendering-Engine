#include "SSRGIPassVulkan.h"
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

SSRGIPassVulkan::SSRGIPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{

}

SSRGIPassVulkan::~SSRGIPassVulkan()
{

}

bool SSRGIPassVulkan::init(WindowConfig config)
{
    PostProcessRendererVulkan::init(config);
        
    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImageHiZ = highZRendererVulkan->outputImages[currentFrame];
    depthImageRaw = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];
    colorImage = deferredRendererVulkan->renderTarget.colorTextures[currentFrame];
    albedoImage = deferredRendererVulkan->renderTarget.gBufferAlbedo[currentFrame];
    pbrImage = deferredRendererVulkan->renderTarget.gPBR[currentFrame];
    emissiveImage = deferredRendererVulkan->renderTarget.gBufferEmissive[currentFrame];

    bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));

    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool SSRGIPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    
    pipeline->destroy();
    
    return true;
}

void SSRGIPassVulkan::onUpdate()
{
    PostProcessRendererVulkan::onUpdate();

}

void SSRGIPassVulkan::render(Camera &camera)
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
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImageHiZ = highZRendererVulkan->outputImages[currentFrame];
    depthImageRaw = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];
    colorImage = deferredRendererVulkan->renderTarget.colorTextures[currentFrame];
    albedoImage = deferredRendererVulkan->renderTarget.gBufferAlbedo[currentFrame];
    pbrImage = deferredRendererVulkan->renderTarget.gPBR[currentFrame];
    emissiveImage = deferredRendererVulkan->renderTarget.gBufferEmissive[currentFrame];
    outputImage = outputImages[currentFrame];

    
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

    ubo.view = camera.getViewMatrix();
    ubo.projection = camera.getProjectionMatrix();
    ubo.projection[1][1] *= -1.0f; 
    ubo.invProj = glm::inverse(ubo.projection);
    ubo.invView = glm::inverse(ubo.view);

	pushConstant.screenRes = { deferredRendererVulkan->renderTarget.width, deferredRendererVulkan->renderTarget.height };
    pushConstant.maxDistance = 500.0f;
    pushConstant.maxMip = 11;          
    pushConstant.thickness = 0.05f;
    pushConstant.time = AppWindow::getTime();
    pushConstant.frameSeed = rand() % 32768;

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    writeSSRGI(cmd, currentFrame);
}

void SSRGIPassVulkan::writeSSRGI(VkCommandBuffer cmd, uint32_t currentFrame)
{
    TextureManagerVulkan::transitionImageLayout(
        cmd, outputImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan);

    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(SSRGISetsID)[currentFrame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        pipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 
        0, sizeof(PushConstant), &pushConstant);

    auto renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    uint32_t width = deferredRendererVulkan->renderTarget.width;
    uint32_t height = deferredRendererVulkan->renderTarget.height;
    uint32_t groupX = (width  + 7) / 8;
    uint32_t groupY = (height + 7) / 8;

    vkCmdDispatch(cmd, groupX, groupY, 1);
    
    TextureManagerVulkan::transitionImageLayout(
        cmd, outputImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);
}

void SSRGIPassVulkan::_createResources()
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
 
    outputImages.resize(VulkanUtils::numFrames());
    for(int i = 0; i < outputImages.size(); i++) {
        createTexture(outputImageID, outputImages[i]);
    }
}

void SSRGIPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(SSRGILayoutID);
    
	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline(
		"assets/shaders/spv/SSRGI.comp.spv", 
		{ layout }, 
		sizeof(PushConstant)
	);
}

void SSRGIPassVulkan::_createDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

	uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 8},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
	};
	
    SSRGILayoutID = descriptorManagerVulkan->createLayout(bindings);
    SSRGIPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
	SSRGISetsID = descriptorManagerVulkan->createSets(SSRGILayoutID, SSRGIPoolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void SSRGIPassVulkan::_updateDescriptor(uint32_t index)
{
    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(renderer);
	
    depthImageHiZ = highZRendererVulkan->outputImages[index];
    depthImageRaw = deferredRendererVulkan->renderTarget.depthTextures[index];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[index];
    colorImage = deferredRendererVulkan->renderTarget.colorTextures[index];
    albedoImage = deferredRendererVulkan->renderTarget.gBufferAlbedo[index];
    pbrImage = deferredRendererVulkan->renderTarget.gPBR[index];
    emissiveImage = deferredRendererVulkan->renderTarget.gBufferEmissive[index];
    outputImage = outputImages[index];

	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(SSRGISetsID);
    VkDescriptorImageInfo depthImageInfo{};
	depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthImageInfo.imageView = depthImageHiZ->textureImageView;
	depthImageInfo.sampler = depthImageHiZ->textureSampler;

    VkDescriptorImageInfo normalImageInfo{};
	normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalImageInfo.imageView = normalImage->textureImageView;
	normalImageInfo.sampler = normalImage->textureSampler;
    
    VkDescriptorImageInfo sceneColorImageInfo{};
	sceneColorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	sceneColorImageInfo.imageView = colorImage->textureImageView;
	sceneColorImageInfo.sampler = colorImage->textureSampler;

	VkDescriptorImageInfo outputImageInfo{};
	outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputImageInfo.imageView = outputImage->textureImageView;
	outputImageInfo.sampler = outputImage->textureSampler;

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[index]);
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo depthImageRawInfo{};
	depthImageRawInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthImageRawInfo.imageView = depthImageRaw->textureImageView;
	depthImageRawInfo.sampler = depthImageRaw->textureSampler;

    VkDescriptorImageInfo albedoImageInfo{};
	albedoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	albedoImageInfo.imageView = albedoImage->textureImageView;
	albedoImageInfo.sampler = albedoImage->textureSampler;

    VkDescriptorImageInfo pbrImageInfo{};
	pbrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	pbrImageInfo.imageView = pbrImage->textureImageView;
	pbrImageInfo.sampler = pbrImage->textureSampler;

    VkDescriptorImageInfo emissiveImageInfo{};
	emissiveImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	emissiveImageInfo.imageView = emissiveImage->textureImageView;
	emissiveImageInfo.sampler = emissiveImage->textureSampler;

    uint32_t imageID = textureManagerVulkan->loadTexture("assets/textures/obluenoise256.png", 1, true);
	TextureVulkan* blueNoiseImage = textureManagerVulkan->getTexture(imageID);
    VkDescriptorImageInfo blueNoiseInfo{};
	blueNoiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	blueNoiseInfo.imageView = blueNoiseImage->textureImageView;
	blueNoiseInfo.sampler = blueNoiseImage->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 0, depthImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 1, normalImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 2, sceneColorImageInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 3, outputImageInfo);
	descriptorManagerVulkan->writeUniform(&writes, descriptorSets[index], 4, bufferInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 5, depthImageRawInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 6, albedoImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 7, pbrImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 8, emissiveImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 9, blueNoiseInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void SSRGIPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();
    _createResources();
	_createDescriptors();
	_createPipelines();
}

void SSRGIPassVulkan::_cleanupResources()
{
    for(int i = 0; i < outputImages.size(); i++) {
        textureManagerVulkan->destroy(outputImages[i]->id());
    }
    pipeline->destroy();
}