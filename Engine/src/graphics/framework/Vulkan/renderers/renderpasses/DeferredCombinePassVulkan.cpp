#include "DeferredCombinePassVulkan.h"
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
#include <graphics/framework/vulkan/renderers/features/DDGIBuilderVulkan.h>
#include <graphics/framework/vulkan/renderers/features/ImageBasedVulkan.h>
#include <core/scene/SceneManager.h>
#include <window/AppWindow.h>
#include <math.h>
#include <algorithm>
#include "TemporalPassVulkan.h"
#include "BloomPassVulkan.h"

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

    // stays null when DDGIBuilderVulkan is not added in RendererManagerVulkan, the pass then combines without it
    RendererVulkan* renderer = rendererManagerVulkan->getRenderer("DDGIBuilderVulkan");
	ddgiRenderer = dynamic_cast<DDGIBuilderVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);

    _fetchInputImages(renderDeviceVulkan->getCurrentFrameIndex());

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
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	const GraphicsSettingsSerializer& graphicsSettings = rendererManagerVulkan->getGraphicsSettings();
	const DeferredSettings* deferredSettings = graphicsSettings.get<DeferredSettings>();
	const DDGISettings* ddgiSettings = graphicsSettings.get<DDGISettings>();	// null without ray tracing, the pass then combines without ddgi

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    _fetchInputImages(currentFrame);
    _createResources();     // same output unless the render size changed

    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    pushConstant.shouldCombine = (deferredSettings && deferredSettings->combine) ? 1 : 0;

    // the grid values are published by the ddgi builder so they always match what the atlases were built with
    pushConstant.ddgiMode = 0;
    if(ddgiSettings && ddgiSettings->isReady()) {
        pushConstant.gridSpacing = ddgiSettings->gridSpacing;
        pushConstant.gridOrigin = ddgiSettings->gridOrigin;
        pushConstant.probeCounts = glm::ivec4(ddgiSettings->probeCounts, 0);
        pushConstant.skyIntensity = ddgiSettings->skyIntensity;
        pushConstant.probesResolution = 8;
        pushConstant.normalBias = ddgiSettings->normalBias;
        pushConstant.ddgiIntensity = ddgiSettings->intensity;
        pushConstant.ddgiMode = ddgiSettings->indirectOnly ? 2 : 1;
    }

    ubo.view = camera.getViewMatrix();
    ubo.projection = camera.getProjectionMatrix();
    ubo.projection[1][1] *= -1.0f; 
    ubo.invProj = glm::inverse(ubo.projection);
    ubo.invView = glm::inverse(ubo.view);
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	pushConstant.screenRes = { geometryPass->renderTarget.width, geometryPass->renderTarget.height };
    pushConstant.time = AppWindow::getTime();
    pushConstant.frameSeed = rand() % 32768;

    _updateDescriptor(currentFrame);

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "Deferred Combine Pass");
    writeCombinedImage(cmd, currentFrame);
	renderDeviceVulkan->endLabel(cmd);

    rendererManagerVulkan->setDisplayImage(outputImage);
}

// the gbuffer is per frame and the gi / ddgi images can be recreated, so these are looked up again every frame
void DeferredCombinePassVulkan::_fetchInputImages(uint32_t currentFrame)
{
    RendererVulkan* renderer = nullptr;
	const DeferredSettings* deferredSettings = rendererManagerVulkan->getGraphicsSettings().get<DeferredSettings>();
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	auto lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    renderer = rendererManagerVulkan->getRenderer("TemporalPassVulkan");
	auto temporalPassRenderer = dynamic_cast<TemporalPassVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("SSRGIPassVulkan");
	auto SSRGIPassRenderer = dynamic_cast<SSRGIPassVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("BloomPassVulkan");
	auto bloomPassRenderer = dynamic_cast<BloomPassVulkan*>(renderer);

    if(deferredSettings && deferredSettings->temporalReprojection) {
        denoisedGIImage = temporalPassRenderer->getOutputImage();
    } else {
        denoisedGIImage = SSRGIPassRenderer->getOutputImage();
    }

    sceneImage = lightingPass->renderTarget.colorTexture;
    albedoImage = geometryPass->renderTarget.gBufferAlbedo;
    positionImage = geometryPass->renderTarget.gBufferPos;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    pbrImage = geometryPass->renderTarget.gPBR;
	bloomImage = bloomPassRenderer->getOutputImage();

    // without the ddgi renderer the shader never samples the atlases (ddgiMode 0), the bindings still need an image
    atlasImageGI = ddgiRenderer ? ddgiRenderer->getAtlasImage() : bloomImage;
    visibilityAtlasImageGI = ddgiRenderer ? ddgiRenderer->getVisibilityAtlasImage() : bloomImage;
}

void DeferredCombinePassVulkan::writeCombinedImage(VkCommandBuffer cmd, uint32_t currentFrame)
{
    // the graph moves outputImage to GENERAL before this node and to SHADER_READ_ONLY at the end of the frame
    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    
    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(setsID)[currentFrame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        pipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 
        0, sizeof(PushConstant), &pushConstant);

	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    uint32_t width = geometryPass->renderTarget.width;
    uint32_t height = geometryPass->renderTarget.height;
    uint32_t groupX = (width  + 15) / 16;
    uint32_t groupY = (height + 15) / 16;

    vkCmdDispatch(cmd, groupX, groupY, 1);
}

void DeferredCombinePassVulkan::_createResources()
{
    RGTextureDesc desc{};
    desc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    uint32_t previousOutputID = outputImageID;
    outputHandle = graph.createRenderTexture("DeferredCombine.Output", desc);
    outputImage = graph.getTexture(outputHandle);
    outputImageID = outputImage->id();

    // compared by id, a recreated texture can land at the same address as the one it replaced
    if(outputImageID != previousOutputID) {
        // ApplicationRendererVulkan draws the display image through the bindless set when the gui is hidden,
        // the debug pass also reads the scene through it
        textureManagerVulkan->registerBindlessTexture(outputImageID);
    }
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
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

	uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 11},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 1},
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

    VkDescriptorImageInfo bloomImageInfo{};
	bloomImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	bloomImageInfo.imageView = bloomImage->textureImageView;
	bloomImageInfo.sampler = bloomImage->textureSampler;

    VkDescriptorImageInfo atlasImageInfo{};
	atlasImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	atlasImageInfo.imageView = atlasImageGI->textureImageView;
	atlasImageInfo.sampler = atlasImageGI->textureSampler;

    VkDescriptorImageInfo positionImageInfo{};
	positionImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	positionImageInfo.imageView = positionImage->textureImageView;
	positionImageInfo.sampler = positionImage->textureSampler;

    VkDescriptorImageInfo pbrImageInfo{};
	pbrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	pbrImageInfo.imageView = pbrImage->textureImageView;
	pbrImageInfo.sampler = pbrImage->textureSampler;

    VkDescriptorImageInfo normalImageInfo{};
	normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalImageInfo.imageView = normalImage->textureImageView;
	normalImageInfo.sampler = normalImage->textureSampler;

    VkDescriptorImageInfo visibilityAtlasImageInfo{};
	visibilityAtlasImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	visibilityAtlasImageInfo.imageView = visibilityAtlasImageGI->textureImageView;
	visibilityAtlasImageInfo.sampler = visibilityAtlasImageGI->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeUniform(&writes, descriptorSets[index], 0, bufferInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 1, outputImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 2, GIImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 3, sceneColorImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 4, albedoImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 5, bloomImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 6, atlasImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 7, positionImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 8, normalImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 9, visibilityAtlasImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 10, pbrImageInfo);
	descriptorManagerVulkan->writeStorage(&writes, descriptorSets[index], 11, imageBasedRenderer->finalSumBuffers[index]->getDescStorageBufferInfo());
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
    pipeline->destroy();
}

void DeferredCombinePassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    // the gi image depends on the denoiser toggle and the taa ping-pong, look it up before importing
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    _fetchInputImages(currentFrame);
    _createResources();     // same output unless the render size changed

    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    auto gi = graph.importTexture(denoisedGIImage);
    auto scene = graph.importTexture(sceneImage);
    auto albedo = graph.importTexture(albedoImage);
    auto bloom = graph.importTexture(bloomImage);
    auto atlas = graph.importTexture(atlasImageGI);
    auto position = graph.importTexture(positionImage);
    auto normal = graph.importTexture(normalImage);
    auto visibility = graph.importTexture(visibilityAtlasImageGI);
    auto pbr = graph.importTexture(pbrImage);

    RenderPassNode node{};
    node.name = "DeferredCombine";
    node.reads = {
        { gi, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { scene, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { bloom, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { atlas, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { position, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { visibility, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { pbr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
    };
    node.writes = {
        { outputHandle, VK_IMAGE_LAYOUT_GENERAL },
    };

    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        Camera* camera = SceneManager::cameraController;
        if(!camera) {
            return;
        }
        render(*camera);
    };

    graph.addPass(node);
}
