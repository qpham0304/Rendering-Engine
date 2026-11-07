#include "AmbientOcclusionPassVulkan.h"
#include "DepthPrepassVulkan.h"
#include "BlurPassVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/camera.h>
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h"
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/AccelStructureBufferVulkan.h>

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

    rtSupported = rendererManagerVulkan->getRenderer("RayTraceRendererVulkan") != nullptr;

    _createOcclusionMap();
    _createDescriptors();
	_createPipelines();
    outputImage = aoMap;    // until the first frame hands back the blurred map

    pushConstant.bias = 0.01f;

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
    glm::mat4 invProj = glm::inverse(proj);
    float fovRadians = glm::radians(camera.getFOV());
    float projScale = static_cast<float>(rendererManagerVulkan->getRenderGraph().getRenderSize().height) / (2.0f * tanf(fovRadians * 0.5f));
    float radius = std::max(settings->radius, 0.01f);

    // ray traced falls back to gtao when this gpu can't trace or the scene has no tlas yet
    Method method = static_cast<Method>(std::clamp(settings->method, 0, 2));
    AccelStructureBufferVulkan* tlas = nullptr;
    if(method == Method::RayTraced) {
        if(rtSupported && rtaoPipeline) {
            auto raytracer = dynamic_cast<RayTraceRendererVulkan*>(rendererManagerVulkan->getRenderer("RayTraceRendererVulkan"));
            raytracer->updateTlas();
            tlas = raytracer->m_rtBuilder.getTlas();
        }
        if(!tlas) {
            method = Method::GTAO;
        }
    }

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "AO Pass");
    if(method == Method::Alchemy) {
        pushConstant.invProj = invProj;
        pushConstant.projScale = projScale;
        pushConstant.radius = radius;
        pushConstant.intensity = settings->intensity;

        _updateDescriptorSetsAO(currentFrame);
        VkDescriptorSet set = descriptorManagerVulkan->getDescriptorSet(occlusionDecriptorsetsID)[currentFrame];
        writeAO(cmd, currentFrame, occlusionPipeline.get(), set, &pushConstant, sizeof(pushConstant));
    }
    else if(method == Method::GTAO) {
        gtaoPush.invProj = invProj;
        gtaoPush.projScale = projScale;
        gtaoPush.radius = radius;
        gtaoPush.intensity = settings->intensity;
        gtaoPush.sliceCount = 3;
        gtaoPush.stepCount = 6;

        _updateDescriptorSetsAO(currentFrame);
        VkDescriptorSet set = descriptorManagerVulkan->getDescriptorSet(occlusionDecriptorsetsID)[currentFrame];
        writeAO(cmd, currentFrame, gtaoPipeline.get(), set, &gtaoPush, sizeof(gtaoPush));
    }
    else {
        rtaoPush.invViewProj = glm::inverse(proj * camera.getViewMatrix());
        rtaoPush.cameraPos = glm::vec4(camera.getPosition(), 1.0f);
        rtaoPush.radius = radius;
        rtaoPush.intensity = settings->intensity;
        rtaoPush.rayCount = std::clamp(settings->rayCount, 1, 64);

        _updateDescriptorSetsRT(currentFrame, tlas->getDescAccelStructInfo());
        VkDescriptorSet set = descriptorManagerVulkan->getDescriptorSet(rtSetsID)[currentFrame];
        writeAO(cmd, currentFrame, rtaoPipeline.get(), set, &rtaoPush, sizeof(rtaoPush));
    }
	renderDeviceVulkan->endLabel(cmd);
}

void AmbientOcclusionPassVulkan::writeAO(VkCommandBuffer cmd, uint32_t currentFrame, VulkanPipeline* pipeline, VkDescriptorSet set, const void* push, uint32_t pushSize)
{
    VkExtent2D extent = rendererManagerVulkan->getRenderGraph().getRenderSize();
    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushSize, push);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipelineLayout, 0, 1, &set, 0, nullptr);

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

    // own set for the ray traced method, the tlas binding can't live in the update after bind layout above
    if(rtSupported) {
        rtBindings = {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
            { 2, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        };
        std::vector<VkDescriptorPoolSize> rtPoolSizes {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount },
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frameCount },
        };
        rtLayoutID = descriptorManagerVulkan->createLayout(rtBindings);
        rtPoolID = descriptorManagerVulkan->createPool(rtPoolSizes, frameCount);
        rtSetsID = descriptorManagerVulkan->createSets(rtLayoutID, rtPoolID, frameCount);
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

	gtaoPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	gtaoPipeline->createComputePipeline(
		"assets/shaders/spv/gtao.comp.spv",
		{ descriptorLayoutLUT },
		sizeof(GTAOPushConstant)
	);

    if(rtSupported) {
        VkDescriptorSetLayout rtLayout = descriptorManagerVulkan->getDescriptorLayout(rtLayoutID);
        rtaoPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
        rtaoPipeline->createComputePipeline(
            "assets/shaders/spv/rtao.comp.spv",
            { rtLayout },
            sizeof(RTAOPushConstant)
        );
    }
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

// written every frame it is used, the tlas is rebuilt when the instance count changes
void AmbientOcclusionPassVulkan::_updateDescriptorSetsRT(uint32_t index, const VkWriteDescriptorSetAccelerationStructureKHR& tlasInfo)
{
	auto sets = descriptorManagerVulkan->getDescriptorSet(rtSetsID);

	VkDescriptorImageInfo aoImageInfo{ aoMap->textureSampler, aoMap->textureImageView, VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo depthImageInfo{ depthImage->textureSampler, depthImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, sets[index], 0, aoImageInfo);
	descriptorManagerVulkan->writeImage(&writes, sets[index], 1, depthImageInfo);
	descriptorManagerVulkan->writeAccelStruct(&writes, sets[index], 2, rtBindings, tlasInfo);
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
    gtaoPipeline->destroy();
    if(rtaoPipeline) {
        rtaoPipeline->destroy();
    }
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

    // blurType 0 is no blur, the rest map onto BlurType
    int blurType = std::clamp(settings->blurType, 0, 3);
    if(blurType == 0) {
        outputImage = aoMap;
        return;
    }

    Camera* camera = SceneManager::cameraController;
    BlurPassVulkan::BlurParams blurParams{};
    blurParams.type = static_cast<BlurType>(blurType - 1);
    blurParams.depth = depthHandle;
    if(camera) {
        glm::mat4 proj = camera->getProjectionMatrix();
        proj[1][1] *= -1.0f;
        blurParams.invProj = glm::inverse(proj);
    }
    blurParams.radius = std::max(settings->blurRadius, 1);
    blurParams.sigma = static_cast<float>(blurParams.radius);
    blurParams.depthScale = settings->blurScale;

    RGHandle blurredHandle = blurPass->blur(graph, "AO", aoMapHandle, blurParams);
    outputImage = graph.getTexture(blurredHandle);
}
