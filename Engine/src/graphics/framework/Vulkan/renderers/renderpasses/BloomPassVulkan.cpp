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

BloomPassVulkan::BloomPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{
    setFootprint("BloomPassVulkan", sizeof(BloomPassVulkan), MemorySystem::Graphics);

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
	auto lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));

    assert(giPassVulkan && "failed to retrieve GI Pass data");

    //use swapchain size not a potential 0x0 minimized appwindow size, also to match render image
    uint32_t width = renderDeviceVulkan->swapchain.swapChainExtent.width;
    uint32_t height = renderDeviceVulkan->swapchain.swapChainExtent.height;
    uint32_t maxMips = std::floor(std::log2(std::max(width, height))) - 2;
    mipCount = std::clamp(maxMips, 5u, 10u);

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    inputImage = lightingPass->renderTarget.colorTexture;
    
    lensDirtTextureID = textureManagerVulkan->loadTexture("assets/textures/DirtMaskTexture.jpg", 1, true);
    
    _createResources();
    _createDescriptors();
	_createPipelines();

    settings = &rendererManagerVulkan->getGraphicsSettings().add<BloomSettings>("Bloom");

    return true;
}

bool BloomPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    rendererManagerVulkan->getGraphicsSettings().remove<BloomSettings>();
    settings = nullptr;

    _cleanupResources();
    _destroyMipViews();

    return true;
}
void BloomPassVulkan::onUpdate() 
{
    PostProcessRendererVulkan::onUpdate();
    
}

void BloomPassVulkan::render(Camera& camera) {
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    auto lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    inputImage = lightingPass->renderTarget.colorTexture;

    if (needResize) {
        _recreateResources();
        needResize = false;
        return;
    }

    downPushConstant.threshold = settings->threshold;
    downPushConstant.softThreshold = settings->softThreshold;
    bloomPushConstant.bloomStrength = settings->strength;
    bloomPushConstant.exposure = settings->exposure;
    bloomPushConstant.dirtImageIdx = settings->lensDirt ? lensDirtTextureID : 0;

    VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    renderDeviceVulkan->beginLabel(cmd, "Bloom Pass");
    writeDownSample(cmd, currentFrame);
    writeUpSample(cmd, currentFrame);
    writeBloomProcess(cmd, currentFrame);
    renderDeviceVulkan->endLabel(cmd);
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
    RGTextureDesc desc{};
    desc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    desc.config.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    desc.config.mipLevels = mipCount;

    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    uint32_t previousOutputID = outputImageID;
    outputHandle = graph.createRenderTexture("Bloom.Chain", desc);
    outputImage = graph.getTexture(outputHandle);
    outputImageID = outputImage->id();

    // compared by id, a recreated texture can land at the same address and the old mip views would point at freed memory
    if(outputImageID == previousOutputID) {
        return;
    }

    // a new texture, rebuild the per mip views the down/up sample passes write through
    _destroyMipViews();

    mipChain.resize(mipCount);
    mipSizes.resize(mipCount);
    glm::vec2 currentSize(outputImage->width(), outputImage->height());

    for (uint32_t i = 0; i < mipCount; i++) {
        TextureManagerVulkan::createImageView(
            outputImage->textureImage, mipChain[i],
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
            1, i, 1, VK_IMAGE_VIEW_TYPE_2D, renderDeviceVulkan->device
        );
        mipSizes[i] = currentSize;
        currentSize = glm::max(glm::vec2(1.0f), glm::floor(currentSize / 2.0f));
    }
}

void BloomPassVulkan::_destroyMipViews()
{
    for(auto& view : mipChain) {
        vkDestroyImageView(renderDeviceVulkan->device, view, nullptr);
    }
    mipChain.clear();
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
    auto lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));

    for(int i = 0; i < VulkanUtils::numFrames(); i++) {
        std::vector<VkWriteDescriptorSet> writes;

        VkDescriptorImageInfo bloomInfo{ outputImage->textureSampler, mipChain[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkDescriptorImageInfo outInfo{ VK_NULL_HANDLE, mipChain[0], VK_IMAGE_LAYOUT_GENERAL };
        VkDescriptorImageInfo dirtInfo{ dirtTex->textureSampler, dirtTex->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        // set i is bound on frame i, so it reads frame i's scene color
        TextureVulkan* sceneImage = lightingPass->renderTarget.colorTexture;
        VkDescriptorImageInfo sceneInfo{ sceneImage->textureSampler, sceneImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

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
}

void BloomPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    auto lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    inputImage = lightingPass->renderTarget.colorTexture;
    _createResources();     // same chain unless the render size changed

    if (needResize) {
        _recreateResources();
        needResize = false;
        return;
    }

    auto scene = graph.importTexture(inputImage);

    RenderPassNode node{};
    node.name = "Bloom";
    node.reads  = { { scene, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };
    // each mip goes GENERAL and back inside the down/up sample loops, the chain rests in SHADER_READ_ONLY
    node.writes = { { outputHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };

    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        Camera* camera = SceneManager::cameraController;
        if(!camera) {
            return;
        }
        render(*camera);
    };

    graph.addPass(node);
}
