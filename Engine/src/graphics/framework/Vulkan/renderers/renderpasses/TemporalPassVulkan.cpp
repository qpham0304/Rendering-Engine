#include "TemporalPassVulkan.h"
#include <core/scene/SceneManager.h>
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
    setFootprint("TemporalPassVulkan", sizeof(TemporalPassVulkan), MemorySystem::Graphics);

}

TemporalPassVulkan::~TemporalPassVulkan()
{

}

bool TemporalPassVulkan::init(WindowConfig config)
{
    PostProcessRendererVulkan::init(config);
        
    RendererVulkan* renderer = nullptr;
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(renderer);
    renderer = rendererManagerVulkan->getRenderer("SSRGIPassVulkan");
	auto SSRGIPassRenderer = dynamic_cast<SSRGIPassVulkan*>(renderer);

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    ssrgiImage = SSRGIPassRenderer->getOutputImage();
    ssrgiSpecularImage = SSRGIPassRenderer->getSpecularImage();
    motionImage = geometryPass->renderTarget.gBufferMotion;
    depthImage = geometryPass->renderTarget.depthTexture;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    pbrImage = geometryPass->renderTarget.gPBR;

    bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));

    _createResources();
    _createDescriptors();
    _createPipelines();

    settings = &rendererManagerVulkan->getGraphicsSettings().add<TemporalSettings>("Temporal Denoise");

    return true;
}

bool TemporalPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    
    pipeline->destroy();
    rendererManagerVulkan->getGraphicsSettings().remove<TemporalSettings>();
    settings = nullptr;
    
    return true;
}

void TemporalPassVulkan::onUpdate()
{
    PostProcessRendererVulkan::onUpdate();

}

void TemporalPassVulkan::render(Camera &camera)
{
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    _updateDescriptor(TemporalSetsID, currentFrame, true);
    _updateDescriptor(TemporalSetsHistoryID, currentFrame, false);
    
    glm::mat4 currentView = camera.getViewMatrix();
    glm::mat4 currentProj = camera.getProjectionMatrix();
    currentProj[1][1] *= -1;
    glm::mat4 currentViewProj = currentProj * currentView;
    ubo.invViewProj = glm::inverse(currentViewProj);
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	pushConstant.invProj = glm::inverse(currentProj);
	pushConstant.screenRes = { geometryPass->renderTarget.width, geometryPass->renderTarget.height };
    pushConstant.maxHistoryRough = std::max(settings->maxHistoryRough, 1.0f);
    pushConstant.maxHistorySmooth = std::max(settings->maxHistorySmooth, 1.0f);
    pushConstant.maxHistoryMoving = std::max(settings->maxHistoryMoving, 1.0f);
    pushConstant.clampGamma = std::max(settings->clampGamma, 0.0f);
    pushConstant.depthTolerance = std::max(settings->depthTolerance, 0.0f);
    pushConstant.normalThreshold = std::clamp(settings->normalThreshold, -1.0f, 1.0f);

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    renderDeviceVulkan->beginLabel(cmd, "Temporal Reprojection Pass");
    writeTP(cmd, currentFrame);
    renderDeviceVulkan->endLabel(cmd);

    ubo.priorViewProj = currentViewProj;
}

void TemporalPassVulkan::writeTP(VkCommandBuffer cmd, uint32_t currentFrame)
{
    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
        
    // flip is toggled in addToGraph, so the graph prepares the same target this writes
    TextureVulkan* currentWriteTarget = flip ? outputImage : ssrgiHistoryImage;
    TextureVulkan* currentReadHistory = flip ? ssrgiHistoryImage : outputImage;

    uint32_t currentSets = flip ? TemporalSetsID : TemporalSetsHistoryID;
    auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(currentSets);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        pipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 
        0, sizeof(PushConstant), &pushConstant);

	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    uint32_t width = geometryPass->renderTarget.width;
    uint32_t height = geometryPass->renderTarget.height;
    uint32_t groupX = (width  + 15) / 16;
    uint32_t groupY = (height + 15) / 16;

    vkCmdDispatch(cmd, groupX, groupY, 1);
    
    _copyImageReource(cmd);
}

// writeTP alternates its target between outputImage and ssrgiHistoryImage,
// this is whichever one holds the newest result
TextureVulkan* TemporalPassVulkan::getOutputImage()
{
    return flip ? outputImage : ssrgiHistoryImage;
}

TextureVulkan* TemporalPassVulkan::getSpecularOutputImage()
{
    return flip ? specularA : specularB;
}

void TemporalPassVulkan::writeTAA(VkCommandBuffer cmd, uint32_t currentFrame)
{
    
}

void TemporalPassVulkan::_createResources()
{
    // all four are owned by the render graph: same textures every frame, recreated when the render size changes
    // (the accumulated history simply restarts after a resize)
    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    auto createTexture = [&](const char* name, VkFormat format, VkImageAspectFlagBits aspect, VkImageUsageFlags extraUsage, RGHandle& handle) {
        RGTextureDesc desc{};
        desc.config.format = format;
        desc.config.aspectBits = aspect;
        desc.config.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | extraUsage;
        handle = graph.createRenderTexture(name, desc);
        return graph.getTexture(handle);
    };

    VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
    outputImage = createTexture("TAA.HistoryA", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, historyAHandle);
    ssrgiHistoryImage = createTexture("TAA.HistoryB", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, historyBHandle);
    prevDepthImage = createTexture("TAA.PrevDepth", depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT, prevDepthHandle);
    prevNormalImage = createTexture("TAA.PrevNormal", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT, prevNormalHandle);
    specularA = createTexture("TAA.SpecularA", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, specularAHandle);
    specularB = createTexture("TAA.SpecularB", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, specularBHandle);
    lengthA = createTexture("TAA.LengthA", VK_FORMAT_R16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, lengthAHandle);
    lengthB = createTexture("TAA.LengthB", VK_FORMAT_R16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, lengthBHandle);

    outputImageID = outputImage->id();
    ssrgiHistoryImageID = ssrgiHistoryImage->id();
    prevDepthImageID = prevDepthImage->id();
    prevNormalImageID = prevNormalImage->id();
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
		{ 9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 10, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 13, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

	uint32_t frameCount = VulkanUtils::numFrames();
    uint32_t totalSetsNeeded = frameCount * 2;
    std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 3 * totalSetsNeeded},
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 11 * totalSetsNeeded},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1  * totalSetsNeeded},
	};
	
    TemporalLayoutID = descriptorManagerVulkan->createLayout(bindings);
    TemporalPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount * totalSetsNeeded);
	TemporalSetsID = descriptorManagerVulkan->createSets(TemporalLayoutID, TemporalPoolID, frameCount);
	TemporalSetsHistoryID = descriptorManagerVulkan->createSets(TemporalLayoutID, TemporalPoolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(TemporalSetsID, i, true);
        _updateDescriptor(TemporalSetsHistoryID, i, false);
    }
}

// set A writes the A textures and reads last frame's B, the other set the other way around
void TemporalPassVulkan::_updateDescriptor(uint32_t currentSetsID, uint32_t index, bool writeA)
{
	TextureVulkan* writeImg = writeA ? outputImage : ssrgiHistoryImage;
	TextureVulkan* readImage = writeA ? ssrgiHistoryImage : outputImage;
	TextureVulkan* specularWrite = writeA ? specularA : specularB;
	TextureVulkan* specularRead = writeA ? specularB : specularA;
	TextureVulkan* lengthWrite = writeA ? lengthA : lengthB;
	TextureVulkan* lengthRead = writeA ? lengthB : lengthA;
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

    VkDescriptorImageInfo pbrImageInfo{};
	pbrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	pbrImageInfo.imageView = pbrImage->textureImageView;
	pbrImageInfo.sampler = pbrImage->textureSampler;

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
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 9, pbrImageInfo);
	VkDescriptorImageInfo specularWriteInfo{ specularWrite->textureSampler, specularWrite->textureImageView, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo ssrgiSpecularInfo{ ssrgiSpecularImage->textureSampler, ssrgiSpecularImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo specularReadInfo{ specularRead->textureSampler, specularRead->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo lengthWriteInfo{ lengthWrite->textureSampler, lengthWrite->textureImageView, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo lengthReadInfo{ lengthRead->textureSampler, lengthRead->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 10, specularWriteInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 11, ssrgiSpecularInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 12, specularReadInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 13, lengthWriteInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 14, lengthReadInfo);
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
    pipeline->destroy();
}

void TemporalPassVulkan::_copyImageReource(VkCommandBuffer cmd) {
    TextureManagerVulkan::copyImage(cmd, depthImage, prevDepthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, renderDeviceVulkan);
    TextureManagerVulkan::copyImage(cmd, normalImage, prevNormalImage, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, renderDeviceVulkan);
}

void TemporalPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	auto SSRGIPassRenderer = dynamic_cast<SSRGIPassVulkan*>(rendererManagerVulkan->getRenderer("SSRGIPassVulkan"));

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    ssrgiImage = SSRGIPassRenderer->getOutputImage();
    ssrgiSpecularImage = SSRGIPassRenderer->getSpecularImage();
    motionImage = geometryPass->renderTarget.gBufferMotion;
    depthImage = geometryPass->renderTarget.depthTexture;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    pbrImage = geometryPass->renderTarget.gPBR;
    _createResources();     // same textures unless the render size changed

    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    flip = !flip;
    RGHandle currentWriteTargetHandle = flip ? historyAHandle : historyBHandle;
    RGHandle historyHandle = flip ? historyBHandle : historyAHandle;	// keeps its contents, last frame's result
    RGHandle specularWriteHandle = flip ? specularAHandle : specularBHandle;
    RGHandle specularHistoryHandle = flip ? specularBHandle : specularAHandle;
    RGHandle lengthWriteHandle = flip ? lengthAHandle : lengthBHandle;
    RGHandle lengthHistoryHandle = flip ? lengthBHandle : lengthAHandle;

    auto ssrgiImageHandle = graph.importTexture(ssrgiImage);
    auto ssrgiSpecularHandle = graph.importTexture(ssrgiSpecularImage);
    auto motionImageHandle = graph.importTexture(motionImage);
    auto depthImageHandle = graph.importTexture(depthImage);
    auto normalImageHandle = graph.importTexture(normalImage);
    auto pbrImageHandle = graph.importTexture(pbrImage);

    RenderPassNode node{};
    node.settings = settings;
    node.name = "TAA";
    node.reads = {
        { ssrgiImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { motionImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { depthImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { normalImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { pbrImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { historyHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { ssrgiSpecularHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { specularHistoryHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { lengthHistoryHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
    };
    
    node.writes = {
        { currentWriteTargetHandle, VK_IMAGE_LAYOUT_GENERAL },
        { specularWriteHandle, VK_IMAGE_LAYOUT_GENERAL },
        { lengthWriteHandle, VK_IMAGE_LAYOUT_GENERAL },
        { prevDepthHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { prevNormalHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
    };

    node.execute = [this] (VkCommandBuffer cmd, uint32_t currentFrame){
        Scene* scene = SceneManager::getInstance().getActiveScene();
        Camera* camera = SceneManager::cameraController;

        if(!camera) {
            return;
        }

        render(*camera);
    };

    graph.addPass(node);
}
