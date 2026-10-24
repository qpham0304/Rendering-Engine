#include "AmbientOcclusionPassVulkan.h"
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

AmbientOcclusionPassVulkan::AmbientOcclusionPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{
    setFootprint("AmbientOcclusionPassVulkan", sizeof(AmbientOcclusionPassVulkan), MemorySystem::Graphics);

}

AmbientOcclusionPassVulkan::~AmbientOcclusionPassVulkan() 
{

}

bool AmbientOcclusionPassVulkan::init(WindowConfig config) 
{
    // hack to preven double initilization by the deferred renderer and dependency cycle
	// a depth prepass before gbuffer pass which will resolve the issue
    if (m_initialized) {
        return true;
    }
    m_initialized = true;

    PostProcessRendererVulkan::init(config);
    
    RendererVulkan* renderer = nullptr;
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = geometryPass->renderTarget.depthTexture;
    positionImage = geometryPass->renderTarget.gBufferPos;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    
    _createOcclusionMap();
    _createDescriptors();
	_createPipelines();
    
    pushConstant.radius = 0.75f;
    pushConstant.bias = 0.01f;
    pushConstant.intensity = 2.0f;
    
    settings = &rendererManagerVulkan->getGraphicsSettings().add<AmbientOcclusionSettings>("Ambient Occlusion");

    return true;
}
bool AmbientOcclusionPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    rendererManagerVulkan->getGraphicsSettings().remove<AmbientOcclusionSettings>();
    settings = nullptr;

    _cleanupResources();

    return true;
}
void AmbientOcclusionPassVulkan::onUpdate() 
{
    PostProcessRendererVulkan::onUpdate();

}

void AmbientOcclusionPassVulkan::render(Camera& camera)
{
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();

	pushConstant.view = camera.getViewMatrix();
    pushConstant.projScale = camera.getViewHeight() / (2.0 * tan(camera.getFOV() / 2));
    
    float fovRadians = glm::radians(camera.getFOV()); 
    pushConstant.projScale = static_cast<float>(camera.getViewHeight()) / (2.0f * tanf(fovRadians * 0.5f));


	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "AO Pass");
    writeAO(cmd, currentFrame);
    writeBlur(cmd, currentFrame);
	renderDeviceVulkan->endLabel(cmd);
}

void AmbientOcclusionPassVulkan::writeAO(VkCommandBuffer cmd, uint32_t currentFrame)
{
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
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    uint32_t groupX = (geometryPass->renderTarget.width + 15) / 16;
    uint32_t groupY = (geometryPass->renderTarget.height + 15) / 16;
	vkCmdDispatch(cmd, groupX, groupY, 1);
}

void AmbientOcclusionPassVulkan::writeBlur(VkCommandBuffer cmd, uint32_t currentFrame)
{
    VkImageMemoryBarrier aoBarrier{};
    aoBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    aoBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    aoBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    aoBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    aoBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    aoBarrier.image = aoMap->textureImage;
    aoBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &aoBarrier);

    _updateDescriptorSetsBlur(currentFrame);

    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
    uint32_t groupX = (swapchain.swapChainExtent.width + 15) / 16;
    uint32_t groupY = (swapchain.swapChainExtent.height + 15) / 16;

    blurPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    blurrPushConstant.blurRadius = settings->blurRadius;
    blurrPushConstant.scale = settings->blurScale;
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
}

void AmbientOcclusionPassVulkan::_createOcclusionMap()
{
    RGTextureDesc desc{};
    desc.config.format = VK_FORMAT_R16_SFLOAT;
    desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    aoMapHandle = graph.createRenderTexture("AO.Map", desc);
    aoMapTempHandle = graph.createRenderTexture("AO.Temp", desc);
    aoMap = graph.getTexture(aoMapHandle);
    aoMapTemp = graph.getTexture(aoMapTempHandle);
    aoMapID = aoMap->id();
    aoMapTempID = aoMapTemp->id();

    outputImage = aoMap;
}

void AmbientOcclusionPassVulkan::_createDescriptors()
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

void AmbientOcclusionPassVulkan::_createPipelines()
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

void AmbientOcclusionPassVulkan::_updateDescriptorSetsAO(uint32_t index)
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

void AmbientOcclusionPassVulkan::_updateDescriptorSetsBlur(uint32_t index)
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

void AmbientOcclusionPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();
    _createOcclusionMap();
    _createDescriptors();
	_createPipelines();
}

void AmbientOcclusionPassVulkan::_cleanupResources()
{
    occlusionPipeline->destroy();
    blurPipeline->destroy();
}

void AmbientOcclusionPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    // anything that changes which textures are used must happen before importing
    auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    positionImage = geometryPass->renderTarget.gBufferPos;
    normalImage = geometryPass->renderTarget.gBufferNorm;

    _createOcclusionMap();

    // g-buffer was just recreated and is empty, skip this one frame
    if(needResize) {
        needResize = false;
        return;
    }

    auto pos = graph.importTexture(positionImage);
    auto norm = graph.importTexture(normalImage);

    RenderPassNode node{};
    node.name = "AO";
    node.reads  = { {pos, VK_IMAGE_LAYOUT_GENERAL}, {norm, VK_IMAGE_LAYOUT_GENERAL} };
    node.writes = { {aoMapHandle, VK_IMAGE_LAYOUT_GENERAL}, {aoMapTempHandle, VK_IMAGE_LAYOUT_GENERAL} };

    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        Scene* scene = SceneManager::getInstance().getActiveScene();
        Camera* camera = SceneManager::cameraController;

        if(!camera) {
            return;
        }

        render(*camera);
    };

    graph.addPass(node);
}
