#include "ShadowMapRendererVulkan.h"
#include "ShadowMapRendererVulkan.h"
#include "ShadowMapRendererVulkan.h"
#include "ShadowMapRendererVulkan.h"
#include "ShadowMapRendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "graphics/renderers/RenderDevice.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <gui/GuiManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include <core/scene/SceneManager.h>
#include <imgui.h>
#include <vulkan/vulkan.h>

ShadowMapRendererVulkan::ShadowMapRendererVulkan() 
	: Renderer("ShadowMapRendererVulkan")
{


}

ShadowMapRendererVulkan::~ShadowMapRendererVulkan()
{
}

bool ShadowMapRendererVulkan::init(WindowConfig config)
{
	Service::init(config);

	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	bufferManagerVulkan = &static_cast<BufferManagerVulkan&>(bufferManager);
	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = &static_cast<DescriptorManagerVulkan&>(descriptorManager);

	textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
	meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
	materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
	modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
	guiManager = &ServiceLocator::GetService<GuiManager>("ImGuiManager");

	if (!(
		renderDeviceVulkan &&
		bufferManagerVulkan &&
		descriptorManagerVulkan &&
		textureManager &&
		meshManager &&
		materialManager &&
		modelManager &&
		guiManager
	)) {
		return false;
	}

	_createDepthMap();
	_createShadowRenderPass();
	_createShadowFrameBuffer();
	_createShadowPipeline();
	_createOffscreenViewDescriptorSet();

	return true;
}

bool ShadowMapRendererVulkan::onClose()
{
	Service::onClose();

	shadowPipeline->destroy();
	vkDestroyFramebuffer(renderDeviceVulkan->device, shadowFramebuffer, nullptr);
	vkDestroyRenderPass(renderDeviceVulkan->device, shadowRenderPass, nullptr);

	return false;
}

void ShadowMapRendererVulkan::onUpdate()
{
}

void ShadowMapRendererVulkan::beginFrame()
{
	renderDeviceVulkan->beginFrame();
}

void ShadowMapRendererVulkan::endFrame()
{
	renderDeviceVulkan->endFrame();
}

void ShadowMapRendererVulkan::render(Camera& camera)
{
	//glm::vec3 lightPos = glm::vec3(-2.0f, 4.0f, -1.0f);
	//lightView = camera.getViewMatrix();
	//glm::mat4 lightProjection = camera.getProjectionMatrix();	// light area (Left, Right, Bottom, Top, Near, Far)
	//lightProjection[1][1] *= -1;

	lightPos = glm::vec3(100.0f, 100.0f, 100.0f);
	glm::vec3 lookAtTarget = glm::vec3(0.0f, 0.0f, -25.0f);

	lightView = glm::lookAt(lightPos, lookAtTarget, glm::vec3(0.0f, 1.0f, 0.0f));

	float s = 35.0f;
	glm::mat4 lightProjection = glm::ortho(-s, s, -s, s, -250.0f, 250.0f);
	lightProjection[1][1] *= -1;

	lightSpaceMatrix = lightProjection * lightView;
	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();

	recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());
}

void ShadowMapRendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer, void* pipeline)
{
	uint32_t imageIndex = renderDeviceVulkan->getImageIndex();
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);
	VkRenderPass vulkanRenderPass = static_cast<VkRenderPass>(renderPass);
	VkFramebuffer vulkanFrameBuffer = static_cast<VkFramebuffer>(frameBuffer);
	VulkanPipeline* pipelinePtr = static_cast<VulkanPipeline*>(pipeline);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vulkanRenderPass;
	renderPassInfo.framebuffer = vulkanFrameBuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = { width, height };


	std::array<VkClearValue, 1> clearValues{};
	clearValues[0].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	//basic draw commands
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	pipelinePtr->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = width;  // 2048.0f
	viewport.height = height; // 2048.0f
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { width, height };
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void ShadowMapRendererVulkan::endRecording(void* cmdBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	vkCmdEndRenderPass(commandBuffer);
}

void ShadowMapRendererVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();

	beginRecording(commandBuffer, shadowRenderPass, shadowFramebuffer, shadowPipeline.get());

	for (auto& entity : scene->getEntitiesWith<TransformComponent, ModelComponent>()) {
		auto& transform = entity.getComponent<TransformComponent>();
		auto& modelComp = entity.getComponent<ModelComponent>();

		LightPushConstant push{};
		push.model = transform.getModelMatrix();
		push.lightMVP = lightSpaceMatrix;

		vkCmdPushConstants(
			commandBuffer,
			shadowPipeline->pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(LightPushConstant),
			&push
		);

		const Model* model = modelManager->getModel(modelComp.modelID);
		if (!model) {
			continue;
		}
		
		for (uint32_t meshID : model->meshIDs) {
			const Mesh* mesh = meshManager->getMesh(meshID);
			meshManager->bindMesh(meshID); // Bind vertex/index buffers

			uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
			vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
		}
	}
	endRecording(commandBuffer);
}


void ShadowMapRendererVulkan::_createDepthMap()
{
	VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

	uint32_t depthId = textureManager->createTexture();
	depthMap = static_cast<TextureVulkan*>(textureManager->getTexture(depthId));

	TextureManagerVulkan::createImage(
		width,
		height,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depthMap->textureImage,
		depthMap->textureImageMemory,
		1,
		renderDeviceVulkan->device
	);

	TextureManagerVulkan::createImageView(
		depthMap->textureImage,
		depthMap->textureImageView,
		depthFormat,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		1,
		renderDeviceVulkan->device
	);

	TextureManagerVulkan::createTextureSampler(
		depthMap->textureSampler,
		renderDeviceVulkan->device
	);
}

void ShadowMapRendererVulkan::_createShadowPipeline()
{
	PipelineConfigInfo pipelineConfig = VulkanPipeline::defaultPipelineConfigInfo(0);
	pipelineConfig.colorBlendInfo.attachmentCount = 0;
	pipelineConfig.colorBlendInfo.pAttachments = nullptr;

	pipelineConfig.rasterizationInfo.depthBiasEnable = VK_TRUE;
	pipelineConfig.rasterizationInfo.depthBiasConstantFactor = 1.25f;
	pipelineConfig.rasterizationInfo.depthBiasSlopeFactor = 1.75f;

	//pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
	pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	pipelineConfig.renderPass = shadowRenderPass;

	auto bindingDescription = VulkanDevice::VertexVulkan::getBindingDescription();
	auto attributeDescriptions = VulkanDevice::VertexVulkan::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();


	std::vector<VkDescriptorSetLayout> layouts = { };

	shadowPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	shadowPipeline->createGraphicsPipeline(
		"assets/shaders/shadowMap.vert.spv",
		"assets/shaders/shadowMap.frag.spv",
		pipelineConfig,
		vertexInputInfo,
		layouts,
		sizeof(LightPushConstant)
	);
}

void ShadowMapRendererVulkan::_createShadowRenderPass()
{
	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // store it to sample it later
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// Final layout should be SHADER_READ_ONLY_OPTIMAL so the lighting pass can use it immediately
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference depthReference{};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &depthReference;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &depthAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	vkCreateRenderPass(renderDeviceVulkan->device, &renderPassInfo, nullptr, &shadowRenderPass);
}

void ShadowMapRendererVulkan::_createShadowFrameBuffer()
{
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = shadowRenderPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &depthMap->textureImageView;
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.layers = 1;

	vkCreateFramebuffer(renderDeviceVulkan->device, &framebufferInfo, nullptr, &shadowFramebuffer);
}

void ShadowMapRendererVulkan::_createOffscreenViewDescriptorSet()
{
	const uint32_t MAX_NUM_SETS = 3;				// add more if requires more
	
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> bindings = { samplerLayoutBinding };
	imGuilayoutID = descriptorManagerVulkan->createLayout(bindings);

	std::vector<VkDescriptorPoolSize> poolSizes = { 
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} 
	};
	imGuipoolID = descriptorManagerVulkan->createPool(poolSizes, MAX_NUM_SETS);

	imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
	imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(imGuisetIDs[0])[0];

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = depthMap->textureImageView;
	imageInfo.sampler = depthMap->textureSampler;


	std::vector<VkWriteDescriptorSet> writes = {};
	descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}