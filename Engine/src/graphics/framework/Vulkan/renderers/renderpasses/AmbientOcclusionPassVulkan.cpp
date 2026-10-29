#include "AmbientOcclusionPassVulkan.h"
#include "DepthPrepassVulkan.h"
#include "BlurPassVulkan.h"
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
    PostProcessRendererVulkan::init(config);

	auto depthPrepass = dynamic_cast<DepthPrepassVulkan*>(rendererManagerVulkan->getRenderer("DepthPrepassVulkan"));
    depthImage = depthPrepass->renderTarget.depthTexture;
    depthHandle = depthPrepass->renderTarget.depthHandle;

    blurPass = dynamic_cast<BlurPassVulkan*>(rendererManagerVulkan->getRenderer("BlurPassVulkan"));

    _createOcclusionMap();
    _createDescriptors();
	_createPipelines();
    outputImage = aoMap;    // until the first frame hands back the blurred map

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

    // same y flip as the projection the depth was rendered with
    glm::mat4 proj = camera.getProjectionMatrix();
    proj[1][1] *= -1.0f;
	pushConstant.invProj = glm::inverse(proj);
    pushConstant.projScale = camera.getViewHeight() / (2.0 * tan(camera.getFOV() / 2));
    
    float fovRadians = glm::radians(camera.getFOV()); 
    pushConstant.projScale = static_cast<float>(rendererManagerVulkan->getRenderGraph().getRenderSize().height) / (2.0f * tanf(fovRadians * 0.5f));


	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "AO Pass");
    writeAO(cmd, currentFrame);
	renderDeviceVulkan->endLabel(cmd);
}

void AmbientOcclusionPassVulkan::writeAO(VkCommandBuffer cmd, uint32_t currentFrame)
{
    _updateDescriptorSetsAO(currentFrame);
    
    VkExtent2D extent = rendererManagerVulkan->getRenderGraph().getRenderSize();
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

    uint32_t groupX = (extent.width + 15) / 16;
    uint32_t groupY = (extent.height + 15) / 16;
	vkCmdDispatch(cmd, groupX, groupY, 1);
}

void AmbientOcclusionPassVulkan::_createOcclusionMap()
{
    RGTextureDesc desc{};
    desc.config.format = VK_FORMAT_R16_SFLOAT;
    desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    aoMapHandle = graph.createRenderTexture("AO.Map", desc);
    aoMap = graph.getTexture(aoMapHandle);
    aoMapID = aoMap->id();
}

void AmbientOcclusionPassVulkan::_createDescriptors()
{
    occlusionDescriptorLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }	// depth
	}, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT);

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount },
	};
	
	occlusionDescriptorPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
	occlusionDecriptorsetsID = descriptorManagerVulkan->createSets(occlusionDescriptorLayoutID, occlusionDescriptorPoolID, frameCount);

	for(int i = 0; i < frameCount; i++) {
        _updateDescriptorSetsAO(i);
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
}

void AmbientOcclusionPassVulkan::_updateDescriptorSetsAO(uint32_t index)
{
	auto lutDescriptorSets = descriptorManagerVulkan->getDescriptorSet(occlusionDecriptorsetsID);
	VkDescriptorImageInfo aoImageInfo{};
	aoImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	aoImageInfo.imageView = aoMap->textureImageView;
	aoImageInfo.sampler = aoMap->textureSampler;

    VkDescriptorImageInfo depthImageInfo{};
	depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthImageInfo.imageView = depthImage->textureImageView;
	depthImageInfo.sampler = depthImage->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, lutDescriptorSets[index], 0, aoImageInfo);
	descriptorManagerVulkan->writeImage(&writes, lutDescriptorSets[index], 1, depthImageInfo);
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
}

void AmbientOcclusionPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    // anything that changes which textures are used must happen before importing
    auto depthPrepass = dynamic_cast<DepthPrepassVulkan*>(rendererManagerVulkan->getRenderer("DepthPrepassVulkan"));
    depthImage = depthPrepass->renderTarget.depthTexture;
    depthHandle = depthPrepass->renderTarget.depthHandle;

    _createOcclusionMap();
    needResize = false;

    RenderPassNode node{};
    node.name = "AO";
    node.reads  = { {depthHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL} };
    node.writes = { {aoMapHandle, VK_IMAGE_LAYOUT_GENERAL} };

    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        Scene* scene = SceneManager::getInstance().getActiveScene();
        Camera* camera = SceneManager::cameraController;

        if(!camera) {
            return;
        }

        render(*camera);
    };

    graph.addPass(node);

    Camera* camera = SceneManager::cameraController;
    BlurPassVulkan::BlurParams blurParams{};
    blurParams.type = BlurType::Bilateral;
    blurParams.depth = depthHandle;
    if(camera) {
        glm::mat4 proj = camera->getProjectionMatrix();
        proj[1][1] *= -1.0f;
        blurParams.invProj = glm::inverse(proj);
    }
    blurParams.radius = settings->blurRadius;
    blurParams.sigma = static_cast<float>(settings->blurRadius);
    blurParams.depthScale = settings->blurScale;

    RGHandle blurredHandle = blurPass->blur(graph, "AO", aoMapHandle, blurParams);
    outputImage = graph.getTexture(blurredHandle);
}
