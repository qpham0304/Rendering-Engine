#include "BloomPassVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/camera.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/renderers/renderpasses/SSRGIPassVulkan.h>
#include <window/AppWindow.h>
#include <graphics/framework/Vulkan/renderers/renderpiplines/DeferredRendererVulkan.h>
#include <imgui.h>

BloomPassVulkan::BloomPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{

}

BloomPassVulkan::~BloomPassVulkan() 
{

}

bool BloomPassVulkan::init(WindowConfig config) 
{
    PostProcessRendererVulkan::init(config);
    
    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("SSRGIPassVulkan");
	auto giPassVulkan = dynamic_cast<SSRGIPassVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);

    assert(giPassVulkan && "failed to retrieve GI Pass data");

    uint32_t width = AppWindow::getWidth();
    uint32_t height = AppWindow::getHeight();
    uint32_t maxMips = std::floor(std::log2(std::max(width, height))) - 2;
    mipCount = std::clamp(maxMips, 5u, 10u);

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    inputImage = deferredRendererVulkan->renderTarget.colorTextures[currentFrame];
    
    lensDirtTextureID = textureManagerVulkan->loadTexture("assets/textures/DirtMaskTexture.jpg", 1, true);
    
    _createResources();
    _createDescriptors();
	_createPipelines();

    downPushConstant.threshold = 0.5f;
    downPushConstant.softThreshold = 0.5f;

    bloomPushConstant.dirtImageIdx = lensDirtTextureID;
    bloomPushConstant.bloomStrength = 0.04f;
    bloomPushConstant.exposure = 1.0f;

    return true;
}

bool BloomPassVulkan::onClose() 
{
    PostProcessRendererVulkan::onClose();

    _cleanupResources();

    return true;
}
void BloomPassVulkan::onUpdate() 
{
    PostProcessRendererVulkan::onUpdate();
    
}

void BloomPassVulkan::render(Camera& camera) {
    if (needResize) {
        _recreateResources();
        needResize = false;
        return;
    }

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    auto deferred = dynamic_cast<DeferredRendererVulkan*>(rendererManagerVulkan->getRenderer("DeferredRendererVulkan"));
    inputImage = deferred->renderTarget.colorTextures[currentFrame];

    VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    writeDownSample(cmd, currentFrame);
    writeUpSample(cmd, currentFrame);
    writeBloomProcess(cmd, currentFrame);
}

void BloomPassVulkan::renderGui()
{
    ImGui::Begin("bloom control");
    ImGui::SliderFloat("bloomStrength", &bloomPushConstant.bloomStrength, 0.0, 1.0);
    ImGui::SliderFloat("exposure", &bloomPushConstant.exposure, 0.0, 1.0);
    ImGui::SliderFloat("threshold", &downPushConstant.threshold, 0.0, 1.0);
    ImGui::SliderFloat("softThreshold", &downPushConstant.softThreshold, 0.0, 1.0);
    ImGui::SliderInt("bloomOn", &bloomPushConstant.dirtImageIdx, 0.0, 30.0);
    ImGui::End();
}

void BloomPassVulkan::writeDownSample(VkCommandBuffer cmd, uint32_t currentFrame) {
    downsamplePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    for (uint32_t i = 0; i < mipCount - 1; i++) {
        _transitionMip(cmd, outputImage->textureImage, i + 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

        downPushConstant.mipLevel = i;
        downPushConstant.srcResolution = mipSizes[i];

        vkCmdPushConstants(cmd, downsamplePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(downPushConstant), &downPushConstant);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, downsamplePipeline->pipelineLayout, 0, 1, &downDescriptorSets[i], 0, nullptr);

        uint32_t groupX = (uint32_t(mipSizes[i + 1].x) + 15) / 16;
        uint32_t groupY = (uint32_t(mipSizes[i + 1].y) + 15) / 16;
        vkCmdDispatch(cmd, groupX, groupY, 1);
        
        _transitionMip(cmd, outputImage->textureImage, i + 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void BloomPassVulkan::writeUpSample(VkCommandBuffer cmd, uint32_t currentFrame) {
    upsamplePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    upPushConstant.filterRadius = 0.005f;

    for (int i = mipCount - 1; i > 0; i--) {
        _transitionMip(cmd, outputImage->textureImage, i - 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        
        vkCmdPushConstants(cmd, upsamplePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(upPushConstant), &upPushConstant);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, upsamplePipeline->pipelineLayout, 0, 1, &upDescriptorSets[i - 1], 0, nullptr);

        uint32_t groupX = (uint32_t(mipSizes[i - 1].x) + 15) / 16;
        uint32_t groupY = (uint32_t(mipSizes[i - 1].y) + 15) / 16;
        vkCmdDispatch(cmd, groupX, groupY, 1);

        _transitionMip(cmd, outputImage->textureImage, i - 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void BloomPassVulkan::writeBloomProcess(VkCommandBuffer cmd, uint32_t currentFrame)
{
    bloomPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    // write the final result back to Mip 0
    _transitionMip(cmd, outputImage->textureImage, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    vkCmdPushConstants(cmd, bloomPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomPushConstant), &bloomPushConstant);
    
    auto bloomSet = descriptorManagerVulkan->getDescriptorSet(bloomDecriptorsetsID)[currentFrame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomPipeline->pipelineLayout, 0, 1, &bloomSet, 0, nullptr);
    auto bindlessSet = &descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomPipeline->pipelineLayout, 1,  1,  bindlessSet, 0,  nullptr);

    uint32_t groupX = (uint32_t(mipSizes[0].x) + 15) / 16;
    uint32_t groupY = (uint32_t(mipSizes[0].y) + 15) / 16;
    vkCmdDispatch(cmd, groupX, groupY, 1);

    _transitionMip(cmd, outputImage->textureImage, 0, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void BloomPassVulkan::_transitionMip(VkCommandBuffer cmd, VkImage image, uint32_t mipLevel, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.image = image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1, 0, 1 };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void BloomPassVulkan::_createResources() {
    uint32_t id = textureManagerVulkan->createTexture();
    outputImage = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
    outputImageID = id;

    uint32_t width = AppWindow::getWidth();
    uint32_t height = AppWindow::getHeight();

    TextureManagerVulkan::createImage(
        width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        outputImage->textureImage, outputImage->textureImageMemory,
        mipCount, renderDeviceVulkan->device
    );

    mipChain.resize(mipCount);
    mipSizes.resize(mipCount);
    glm::vec2 currentSize(width, height);

    for (uint32_t i = 0; i < mipCount; i++) {
        TextureManagerVulkan::createImageView(
            outputImage->textureImage, mipChain[i],
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
            1, i, 1, VK_IMAGE_VIEW_TYPE_2D, renderDeviceVulkan->device
        );
        mipSizes[i] = currentSize;
        currentSize = glm::max(glm::vec2(1.0f), glm::floor(currentSize / 2.0f));
    }
    outputImage->textureImageView = mipChain[0];

    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(renderDeviceVulkan->device, &samplerInfo, nullptr, &outputImage->textureSampler);

    auto cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
    TextureManagerVulkan::transitionImageLayout(
        cmd, outputImage->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        mipCount, 1, renderDeviceVulkan
    );
    renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);
}

void BloomPassVulkan::_createDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> mipBindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

    uint32_t totalMipSets = (mipCount - 1) * 2; 
    mipDescriptorLayoutID = descriptorManagerVulkan->createLayout(mipBindings);
    
    std::vector<VkDescriptorSetLayoutBinding> bloomBindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };
    bloomDescriptorLayoutID = descriptorManagerVulkan->createLayout(bloomBindings);
    
    uint32_t frameCount = VulkanUtils::numFrames();

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, totalMipSets + (frameCount * 2) },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, totalMipSets + frameCount }
    };

    mipDescriptorPoolID = descriptorManagerVulkan->createPool(poolSizes, totalMipSets + frameCount);
    
    uint32_t dsID = descriptorManagerVulkan->createSets(mipDescriptorLayoutID, mipDescriptorPoolID, mipCount - 1);
    uint32_t usID = descriptorManagerVulkan->createSets(mipDescriptorLayoutID, mipDescriptorPoolID, mipCount - 1);
    bloomDecriptorsetsID = descriptorManagerVulkan->createSets(bloomDescriptorLayoutID, mipDescriptorPoolID, frameCount);

    downDescriptorSets = descriptorManagerVulkan->getDescriptorSet(dsID);
    upDescriptorSets = descriptorManagerVulkan->getDescriptorSet(usID);

    for (uint32_t i = 0; i < mipCount - 1; i++) {
        _updateDownSampleDescriptors(i);
        _updateUpSampleDescriptors(i + 1);
    }
    _updateBloomProcessDesciptors();
}
void BloomPassVulkan::_updateDownSampleDescriptors(uint32_t index) {
    VkDescriptorImageInfo inInfo{};
    inInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inInfo.sampler = outputImage->textureSampler;
    inInfo.imageView = (index == 0) ? inputImage->textureImageView : mipChain[index];

    VkDescriptorImageInfo outInfo{};
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outInfo.imageView = mipChain[index + 1];

    std::vector<VkWriteDescriptorSet> writes;
    descriptorManagerVulkan->writeImage(&writes, downDescriptorSets[index], 0, inInfo);
    descriptorManagerVulkan->writeStorageImage(&writes, downDescriptorSets[index], 1, outInfo);
    descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void BloomPassVulkan::_updateUpSampleDescriptors(uint32_t index) 
{
    VkDescriptorImageInfo inInfo{};
    inInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inInfo.sampler = outputImage->textureSampler;
    inInfo.imageView = mipChain[index]; 

    VkDescriptorImageInfo outInfo{};
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outInfo.imageView = mipChain[index - 1]; 

    std::vector<VkWriteDescriptorSet> writes;
    descriptorManagerVulkan->writeImage(&writes, upDescriptorSets[index - 1], 0, inInfo);
    descriptorManagerVulkan->writeStorageImage(&writes, upDescriptorSets[index - 1], 1, outInfo);
    descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void BloomPassVulkan::_updateBloomProcessDesciptors()
{
    auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(bloomDecriptorsetsID);
    TextureVulkan* dirtTex = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(lensDirtTextureID));

    for(int i = 0; i < VulkanUtils::numFrames(); i++) {
        std::vector<VkWriteDescriptorSet> writes;

        VkDescriptorImageInfo bloomInfo{ outputImage->textureSampler, mipChain[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkDescriptorImageInfo outInfo{ VK_NULL_HANDLE, mipChain[0], VK_IMAGE_LAYOUT_GENERAL };
        VkDescriptorImageInfo dirtInfo{ dirtTex->textureSampler, dirtTex->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkDescriptorImageInfo sceneInfo{ inputImage->textureSampler, inputImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 0, bloomInfo);
        descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[i], 1, outInfo);
        descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 2, sceneInfo);
        descriptorManagerVulkan->updateDescriptorSets(&writes);
    }
}

void BloomPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(mipDescriptorLayoutID);
	upsamplePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	upsamplePipeline->createComputePipeline(
		"assets/shaders/spv/bloomUpsampler.comp.spv", 
		{ layout }, 
		sizeof(UpSamplerPushConstant)
	);

	downsamplePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	downsamplePipeline->createComputePipeline(
		"assets/shaders/spv/bloomDownsampler.comp.spv", 
		{ layout }, 
		sizeof(DownSamplerPushConstant)
	);
    
    auto bloomDescriptorLayout = descriptorManagerVulkan->getDescriptorLayout(bloomDescriptorLayoutID);
    auto bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
    auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);
	bloomPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	bloomPipeline->createComputePipeline(
		"assets/shaders/spv/bloom.comp.spv", 
		{ bloomDescriptorLayout, bindlessLayout }, 
		sizeof(BloomPushConstant)
	);
}

void BloomPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();
    _createResources();
    _createDescriptors();
	_createPipelines();
}

void BloomPassVulkan::_cleanupResources()
{
    upsamplePipeline->destroy();
    downsamplePipeline->destroy();
    bloomPipeline->destroy();

    for(auto& view : mipChain) {
        vkDestroyImageView(renderDeviceVulkan->device, view, nullptr);
    }
}