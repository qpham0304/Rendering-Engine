#include "LightingPassVulkan.h"
#include <window/AppWindow.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/scene/SceneManager.h>
#include <core/features/ServiceLocator.h>
#include <core/events/EventManager.h>
#include <core/features/Camera.h>
#include <graphics/renderers/RenderDevice.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/vulkan/renderers/renderpasses/GeometryPassVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/ShadowMapPassVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/AmbientOcclusionPassVulkan.h>
#include <graphics/framework/vulkan/renderers/features/ImageBasedVulkan.h>
#include <graphics/framework/vulkan/renderers/features/DDGIBuilderVulkan.h>

LightingPassVulkan::LightingPassVulkan(std::string serviceName)
	: RendererVulkan(serviceName)
{

}

LightingPassVulkan::~LightingPassVulkan()
{

}

bool LightingPassVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	RendererVulkan* renderer = nullptr;
	renderer = rendererManagerVulkan->getRenderer("GeometryPassVulkan");
	geometryPass = dynamic_cast<GeometryPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ShadowMapPassVulkan");
	shadowMapPass = dynamic_cast<ShadowMapPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("AmbientOcclusionPassVulkan");
	ambientOcclusionPass = dynamic_cast<AmbientOcclusionPassVulkan*>(renderer);

	assert(geometryPass
		&& shadowMapPass
		&& imageBasedRenderer
		&& ambientOcclusionPass
		&& "failed to retrieve renderer"
	);

	_createRenderTarget();

	lights.reserve(numLights);
	size_t lightBufferSize = numLights * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);

	settings = &rendererManagerVulkan->getGraphicsSettings().add<LightingSettings>("Lighting");

	_createLightDescriptor();
	_createLightPipeline();

	return true;
}

bool LightingPassVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();
	rendererManagerVulkan->getGraphicsSettings().remove<LightingSettings>();
	settings = nullptr;

	return true;
}

void LightingPassVulkan::onUpdate()
{

}

void LightingPassVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);

	if(environmentImage != imageBasedRenderer->hdrImage || aoImage != ambientOcclusionPass->getOutputImage()) {
		renderDeviceVulkan->waitIdle();	// the frame in flight still reads the old descriptors
		_updateLightDescriptor();
	}

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	_updateLights(currentFrame);

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "Light Pass");

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = renderTarget.colorTexture->textureImageView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue.color = { 0.15f, 0.15f, 0.15f, 1.0f };

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = { {0, 0}, {renderTarget.width, renderTarget.height} };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	vkCmdBeginRendering(cmd, &renderingInfo);
	_renderLightPass(cmd, currentFrame);
	vkCmdEndRendering(cmd);

	renderDeviceVulkan->endLabel(cmd);

	rendererManagerVulkan->setDisplayImage(renderTarget.colorTexture);
}

void LightingPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
	_createRenderTarget();

	if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}
	
	auto& gBufferTarget = geometryPass->renderTarget;

    auto gBufferPos = graph.importTexture(gBufferTarget.gBufferPos);
    auto gBufferNorm = graph.importTexture(gBufferTarget.gBufferNorm);
    auto gBufferAlbedo = graph.importTexture(gBufferTarget.gBufferAlbedo);
    auto gPBR = graph.importTexture(gBufferTarget.gPBR);
    auto gBufferEmissive = graph.importTexture(gBufferTarget.gBufferEmissive);
    auto gBufferMotion = graph.importTexture(gBufferTarget.gBufferMotion);
    auto depthTexture = graph.importTexture(gBufferTarget.depthTexture);
    auto aoTexture = graph.importTexture(ambientOcclusionPass->getOutputImage());	// written by ao in GENERAL earlier in the graph


	RenderPassNode node{};
    node.name = "lighting";
    node.reads  = { 
		{ gBufferPos, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ gBufferNorm, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ gBufferAlbedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ gPBR, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ gBufferEmissive, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ gBufferMotion, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ depthTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ aoTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
	};

	node.writes = {
		{ renderTarget.colorHandle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
	};

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

void LightingPassVulkan::_updateLights(uint32_t currentFrame)
{
	lights.clear();

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
		return;
	}

	for (auto& entity : scene->getEntitiesWith<TransformComponent>()) {
		if (entity.hasComponent<LightComponent>()) {
			auto& transform = entity.getComponent<TransformComponent>();
			auto& light = entity.getComponent<LightComponent>();
			lights.push_back(LightSSBO(
				light.color,
				glm::vec4(transform.translateVec, 1.0),
				light.intensity * transform.scaleVec.x
			));
		}
	}

	pushConstantLight.color = settings->sunColor * settings->sunIntensity;
	pushConstantLight.skyboxDetail = settings->skyboxDetail;
	pushConstantLight.aoOn = settings->ambientOcclusion ? 1 : 0;
	pushConstantLight.G = settings->phaseG;
	pushConstantLight.scatteringScale = settings->scatteringScale;
	pushConstantLight.bias = settings->shadowBias;
	pushConstantLight.alpha = settings->momentAlpha;
	pushConstantLight.lintstepLow = settings->linstepLow;
	pushConstantLight.linstepHigh = settings->linstepHigh;
	pushConstantLight.litBias = settings->litBias;
	const GraphicsSettingsSerializer& graphicsSettings = rendererManagerVulkan->getGraphicsSettings();
	const ShadowMapSettings* shadowSettings = graphicsSettings.get<ShadowMapSettings>();
	const DDGISettings* ddgiSettings = graphicsSettings.get<DDGISettings>();

	if(shadowSettings) {
		pushConstantLight.direction = glm::vec4(shadowSettings->lightDir, 1.0f);
		pushConstantLight.sunlightMVP = shadowSettings->lightSpaceMatrix;
	}
	pushConstantLight.time = AppWindow::getTime();
	pushConstantLight.numLights = lights.size();

	// with a probe grid the diffuse ambient comes from ddgi in the combine pass, the light pass only keeps the specular ibl
	pushConstantLight.iblDiffuse = (ddgiSettings && ddgiSettings->isReady()) ? 0.0f : 1.0f;

	StorageBufferVulkan* lightSSBO = lightStoragebuffers[currentFrame];
	lightSSBO->update(lights.data(), lights.size() * sizeof(LightSSBO));
}

void LightingPassVulkan::_renderLightPass(VkCommandBuffer cmd, uint32_t currentFrame)
{
	lightingPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport(renderTarget.width, renderTarget.height);
	renderDeviceVulkan->setScissor(renderTarget.width, renderTarget.height);

	auto lightSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		lightingPipeline->pipelineLayout,
		0,
		1,
		&lightSets[currentFrame],
		0, nullptr
	);

	auto lightDescriptorSets_1 = descriptorManagerVulkan->getDescriptorSet(lightSetsID_1);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		lightingPipeline->pipelineLayout,
		1,
		1,
		&lightDescriptorSets_1[currentFrame],
		0,
		nullptr
	);

	vkCmdPushConstants(
		cmd,
		lightingPipeline->pipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(PushConstantLight),
		&pushConstantLight
	);

	vkCmdDraw(cmd, 3, 1, 0, 0);
}

#pragma region setup
void LightingPassVulkan::_createRenderTarget()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	renderTarget.width = swapchain.swapChainExtent.width;
	renderTarget.height = swapchain.swapChainExtent.height;


	RGTextureDesc desc{};
	desc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	desc.config.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
	renderTarget.colorHandle = graph.createRenderTexture("Lighting.Pass", desc);
	renderTarget.colorTexture = graph.getTexture(renderTarget.colorHandle);
}

void LightingPassVulkan::_createLightPipeline()
{
	auto lightDescriptorLayout = descriptorManagerVulkan->getDescriptorLayout(lightLayoutID);
	auto lightDescriptorLayout_1 = descriptorManagerVulkan->getDescriptorLayout(lightLayoutID_1);
	std::vector<VkDescriptorSetLayout> lightLayouts = { lightDescriptorLayout, lightDescriptorLayout_1 };

	// Prepare empty Vertex Input (No vertex buffer used)
	VkPipelineVertexInputStateCreateInfo emptyVertexInput{};
	emptyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	emptyVertexInput.vertexAttributeDescriptionCount = 0;
	emptyVertexInput.pVertexAttributeDescriptions = nullptr;
	emptyVertexInput.vertexBindingDescriptionCount = 0;
	emptyVertexInput.pVertexBindingDescriptions = nullptr;

	// 1 color attachment, no depth
	PipelineConfigInfo lightConfig = VulkanPipeline::defaultPipelineConfigInfo(1);
	lightConfig.renderPass = VK_NULL_HANDLE;
	lightConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
	lightConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

	AttachmentsInfo attachmentsInfo{};
	attachmentsInfo.colorAttachmentFormats = { renderTarget.colorTexture->format };
	attachmentsInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	lightingPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	lightingPipeline->createGraphicsPipelineDynamic(
		"assets/shaders/spv/lightPass.vert.spv",
		"assets/shaders/spv/lightPass.frag.spv",
		lightConfig,
		attachmentsInfo,
		emptyVertexInput,
		lightLayouts,
		sizeof(PushConstantLight)
	);
}

void LightingPassVulkan::_createLightDescriptor()
{
	// set 0: the g-buffer, sampled (it used to be input attachments when lighting was a subpass of the g-buffer pass)
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
	};

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes{
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 5}
	};

	lightLayoutID = descriptorManagerVulkan->createLayout(bindings);
	uint32_t lightPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount * 4);
	lightSetsID = descriptorManagerVulkan->createSets(lightLayoutID, lightPoolID, frameCount);

	lightLayoutID_1 = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
	});

	uint32_t poolIDLayout_1 = descriptorManagerVulkan->createPool(
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 2},
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 6 },
		},
		frameCount
	);

	lightSetsID_1 = descriptorManagerVulkan->createSets(lightLayoutID_1, poolIDLayout_1, VulkanUtils::numFrames());
	_updateLightDescriptor();
}

void LightingPassVulkan::_updateLightDescriptor()
{
	auto& gBuffer = geometryPass->renderTarget;
	auto sampledInfo = [](TextureVulkan* texture) {
		return VkDescriptorImageInfo{ texture->textureSampler, texture->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	};

	auto lightingDescriptorSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
	for(int i = 0; i < VulkanUtils::numFrames(); i++) {
		std::vector<VkWriteDescriptorSet> writes;
		descriptorManagerVulkan->writeImage(&writes, lightingDescriptorSets[i], 0, sampledInfo(gBuffer.gBufferPos));
		descriptorManagerVulkan->writeImage(&writes, lightingDescriptorSets[i], 1, sampledInfo(gBuffer.gBufferNorm));
		descriptorManagerVulkan->writeImage(&writes, lightingDescriptorSets[i], 2, sampledInfo(gBuffer.gBufferAlbedo));
		descriptorManagerVulkan->writeImage(&writes, lightingDescriptorSets[i], 3, sampledInfo(gBuffer.gPBR));
		descriptorManagerVulkan->writeImage(&writes, lightingDescriptorSets[i], 4, sampledInfo(gBuffer.gBufferEmissive));
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}

	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID_1);
	auto uniformbuffersList = rendererManagerVulkan->getBufferUBO();
	for (size_t i = 0; i < VulkanUtils::numFrames(); i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo ssboInfo{};
		ssboInfo.buffer = static_cast<VkBuffer>(*lightStoragebuffers[i]);
		ssboInfo.offset = 0;
		ssboInfo.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = shadowMapPass->depthMap->textureImageView;
		imageInfo.sampler = shadowMapPass->depthMap->textureSampler;

		VkDescriptorImageInfo noiseImageInfo{};
		noiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		noiseImageInfo.imageView = shadowMapPass->blueNoiseImage->textureImageView;
		noiseImageInfo.sampler = shadowMapPass->blueNoiseImage->textureSampler;

		VkDescriptorBufferInfo bufferInfoSH{};
		bufferInfoSH.buffer = static_cast<VkBuffer>(*imageBasedRenderer->finalSumBuffers[i]);
		bufferInfoSH.offset = 0;
		bufferInfoSH.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo brdfLutImageInfo{};
		brdfLutImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		brdfLutImageInfo.imageView = imageBasedRenderer->brdfLUT->textureImageView;
		brdfLutImageInfo.sampler = imageBasedRenderer->brdfLUT->textureSampler;

		VkDescriptorImageInfo prefilterImageInfo{};
		prefilterImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prefilterImageInfo.imageView = imageBasedRenderer->prefilterMap->textureImageView;
		prefilterImageInfo.sampler = imageBasedRenderer->prefilterMap->textureSampler;

		VkDescriptorImageInfo hdrImageInfo{};
		hdrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hdrImageInfo.imageView = imageBasedRenderer->hdrImage->textureImageView;
		hdrImageInfo.sampler = imageBasedRenderer->hdrImage->textureSampler;

		VkDescriptorImageInfo aoImageInfo{};
		aoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		aoImageInfo.imageView = ambientOcclusionPass->getOutputImage()->textureImageView;
		aoImageInfo.sampler = ambientOcclusionPass->getOutputImage()->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 2, imageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 3, noiseImageInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 4, bufferInfoSH);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 5, brdfLutImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 6, prefilterImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 7, hdrImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 8, aoImageInfo);

		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
	environmentImage = imageBasedRenderer->hdrImage;
	aoImage = ambientOcclusionPass->getOutputImage();
}

void LightingPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	rendererManagerVulkan->setDisplayImage(nullptr);
	_cleanupResources();

	_createRenderTarget();
	_updateLightDescriptor();
	_createLightPipeline();
}

void LightingPassVulkan::_cleanupResources()
{
	lightingPipeline->destroy();
}
#pragma endregion setup
