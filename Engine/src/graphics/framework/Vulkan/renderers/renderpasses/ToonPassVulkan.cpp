#include "ToonPassVulkan.h"
#include "DepthPrepassVulkan.h"
#include "LightingPassVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/ServiceLocator.h>
#include <core/features/Camera.h>
#include <animation/AnimationManager.h>
#include <graphics/utils/GraphicsSettings.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>

ToonPassVulkan::ToonPassVulkan(std::string serviceName)
	: RendererVulkan(serviceName)
{
	setFootprint("ToonPassVulkan", sizeof(ToonPassVulkan), MemorySystem::Graphics);
}

ToonPassVulkan::~ToonPassVulkan()
{

}

bool ToonPassVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	depthPrepass = dynamic_cast<DepthPrepassVulkan*>(rendererManagerVulkan->getRenderer("DepthPrepassVulkan"));
	lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));

	_createDescriptor();
	_createPipeline();

	settings = &rendererManagerVulkan->getGraphicsSettings().add<ToonSettings>("Toon");

	return true;
}

bool ToonPassVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	rendererManagerVulkan->getGraphicsSettings().remove<ToonSettings>();
	settings = nullptr;
	_cleanupResources();

	return true;
}

void ToonPassVulkan::onUpdate()
{

}

void ToonPassVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "Toon Pass");

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = lightingPass->renderTarget.colorTexture->textureImageView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageView = depthPrepass->renderTarget.depthTexture->textureImageView;
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = { {0, 0}, {depthPrepass->renderTarget.width, depthPrepass->renderTarget.height} };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;

	vkCmdBeginRendering(cmd, &renderingInfo);
	_renderToon(cmd, currentFrame);
	vkCmdEndRendering(cmd);

	renderDeviceVulkan->endLabel(cmd);
}

void ToonPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
	needResize = false;		// draws into the prepass depth and the lit image at whatever size they are

	RenderPassNode node{};
	node.settings = settings;
	node.name = "Toon";
	node.reads = { { depthPrepass->renderTarget.depthHandle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL } };
	node.writes = { { lightingPass->renderTarget.colorHandle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } };

	node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
		Camera* camera = SceneManager::cameraController;
		if(!camera) {
			return;
		}
		render(*camera);
	};

	graph.addPass(node);
}

void ToonPassVulkan::_renderToon(VkCommandBuffer cmd, uint32_t currentFrame)
{
	toonPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport(depthPrepass->renderTarget.width, depthPrepass->renderTarget.height);
	renderDeviceVulkan->setScissor(depthPrepass->renderTarget.width, depthPrepass->renderTarget.height);

	auto descriptorSets = descriptor.sets();
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, toonPipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
	materialManager->bindMaterial(cmd, (void*)toonPipeline.get());

	const GraphicsSettingsSerializer& graphicsSettings = rendererManagerVulkan->getGraphicsSettings();
	auto animationManager = &ServiceLocator::GetService<AnimationManager>("AnimationManager");

	pushConstant.objectsRef = rendererManagerVulkan->getObjectsAddress();
	pushConstant.boneMatricesRef = bufferManagerVulkan->getVirtualMemoryAddress(animationManager->getAnimationBufferAddressID(currentFrame));
	pushConstant.sunDirection = glm::vec4(sceneSunDirection(graphicsSettings.get<ShadowMapSettings>()), 0.0f);
	pushConstant.sunColor = sceneSunColor(graphicsSettings.get<LightingSettings>(), graphicsSettings.get<SkyAtmosphereSettings>());
	pushConstant.shadowTint = glm::vec4(settings->shadowTint, 1.0f);
	pushConstant.params = glm::vec4(settings->threshold, settings->softness, settings->brightness, 0.0f);

	vkCmdPushConstants(cmd, toonPipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &pushConstant);

	rendererManagerVulkan->drawToonObjects(cmd);
}


#pragma region setup
void ToonPassVulkan::_createDescriptor()
{
	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	};

	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
	};

	descriptor = descriptorManagerVulkan->createDescriptor(bindings, poolSizes, frameCount);
	_updateDescriptor();
}

void ToonPassVulkan::_updateDescriptor()
{
	auto descriptorSets = descriptor.sets();
	auto uniformbuffersList = rendererManagerVulkan->getBufferUBO();

	for (size_t i = 0; i < VulkanUtils::numFrames(); i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}

void ToonPassVulkan::_createPipeline()
{
	PipelineConfigInfo toonConfig = VulkanPipeline::defaultPipelineConfigInfo(1);
	toonConfig.renderPass = VK_NULL_HANDLE;
	// only the surface the prepass kept passes (gl_Position is invariant in both shaders)
	toonConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
	toonConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	AttachmentsInfo attachmentsInfo{};
	attachmentsInfo.colorAttachmentFormats = { lightingPass->renderTarget.colorTexture->format };
	attachmentsInfo.depthAttachmentFormat = depthPrepass->renderTarget.depthFormat;
	attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkDescriptorSetLayout descriptorSetLayout = descriptor.layout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(textureManagerVulkan->getBindlessTextureLayout());

	toonPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	toonPipeline->createGraphicsPipelineDynamic(
		"assets/shaders/spv/toon.vert.spv",
		"assets/shaders/spv/toon.frag.spv",
		toonConfig,
		attachmentsInfo,
		vertexInputInfo,
		{ descriptorSetLayout, bindlessLayout },
		sizeof(PushConstant)
	);
}

void ToonPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();
	_createPipeline();
}

void ToonPassVulkan::_cleanupResources()
{
	toonPipeline->destroy();
}
#pragma endregion setup
