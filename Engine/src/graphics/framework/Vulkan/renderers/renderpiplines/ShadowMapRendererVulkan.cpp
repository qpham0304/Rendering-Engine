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
	_createShadowPipeline();
	_createOffscreenViewDescriptorSet();

	_createMomentImage();
	_createMomentDescriptor();
	_createShadowFrameBuffer();
	_createComputePipeline();

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
	glm::vec3 lightPos = glm::vec3(5.0f);
	glm::vec3 orientation = glm::vec3(-5.0f);
	lightView = glm::lookAt(lightPos, lightPos + orientation, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 lightProjection = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 25.0f);
	lightProjection[1][1] *= -1;

	//lightPos = glm::vec3(100.0f, 100.0f, 100.0f);
	//glm::vec3 lookAtTarget = glm::vec3(0.0f, 0.0f, -25.0f);
	//lightView = glm::lookAt(lightPos, lookAtTarget, glm::vec3(0.0f, 1.0f, 0.0f));
	//float s = 35.0f;
	//glm::mat4 lightProjection = glm::ortho(-s, s, -s, s, -250.0f, 250.0f);
	//lightProjection[1][1] *= -1;

	lightSpaceMatrix = lightProjection * lightView;
	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();

	recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());

	dispatchBlur(cmdBuffer, renderDeviceVulkan->getImageIndex());
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


	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	//basic draw commands
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	pipelinePtr->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = width;
	viewport.height = height;
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
			meshManager->bindMesh(meshID);

			uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
			vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
		}
	}
	endRecording(commandBuffer);
}

void ShadowMapRendererVulkan::dispatchBlur(VkCommandBuffer cmd, uint32_t frameIndex) 
{
	TextureManagerVulkan::createBarrier(cmd, momentImage->textureImage,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	TextureManagerVulkan::createBarrier(cmd, tempMomentImage->textureImage,
		0, VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipeline);

	// horizontal pass moment -> temp
	ComputePushConstant push{};
	push.isVertical = 0;
	vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstant), &push);

	auto& setsH = descriptorManagerVulkan->getDescriptorSet(compDescSetMtoT_ID);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &setsH[0], 0, nullptr);


	uint32_t groupsX = (width + 15) / 16;
	uint32_t groupsY = (height + 15) / 16;

	//vkCmdDispatch(cmd, (width + 127) / 128, height, 1);
	//vkCmdDispatch(cmd, width, (height + 127) / 128, 1);
	vkCmdDispatch(cmd, groupsX, groupsY, 1);

	// 2. BARRIER: Wait for Horizontal to finish before Vertical starts
	TextureManagerVulkan::createBarrier(cmd, tempMomentImage->textureImage,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	// verticle pass tmp -> moment
	push.isVertical = 1;
	vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstant), &push);

	auto& setsV = descriptorManagerVulkan->getDescriptorSet(compDescSetTtoM_ID);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &setsV[0], 0, nullptr);

	//vkCmdDispatch(cmd, width, (height + 127) / 128, 1);
	vkCmdDispatch(cmd, groupsX, groupsY, 1);

	TextureManagerVulkan::createBarrier(cmd, momentImage->textureImage,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
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
	
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	
	pipelineConfig.colorBlendAttachments.push_back(colorBlendAttachment);
	
	pipelineConfig.colorBlendInfo.attachmentCount = pipelineConfig.colorBlendAttachments.size();
	pipelineConfig.colorBlendInfo.pAttachments = pipelineConfig.colorBlendAttachments.data();

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
	VkAttachmentDescription momentAttachment{};
	momentAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	momentAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	momentAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	momentAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	momentAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// set to GENERAL or COLOR_ATTACHMENT_OPTIMAL so Compute can use it
	momentAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference momentReference{};
	momentReference.attachment = 0;
	momentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
    depthAttachment.format = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // We only need moments now
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthReference{};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &momentReference;
	subpass.pDepthStencilAttachment = &depthReference;

	std::array<VkAttachmentDescription, 2> attachments = { momentAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(renderDeviceVulkan->device, &renderPassInfo, nullptr, &shadowRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow render pass!");
	}
}

void ShadowMapRendererVulkan::_createShadowFrameBuffer()
{
	std::array<VkImageView, 2> attachments = {
		momentImage->textureImageView,
		depthMap->textureImageView
	};


	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = shadowRenderPass;
	framebufferInfo.attachmentCount = attachments.size();
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(renderDeviceVulkan->device, &framebufferInfo, nullptr, &shadowFramebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow framebuffer!");
	}
}

void ShadowMapRendererVulkan::_createMomentImage()
{
	auto createTexture = [&](TextureVulkan** outTexture) {
		uint32_t imageID = textureManager->createTexture();
		*outTexture = static_cast<TextureVulkan*>(textureManager->getTexture(imageID));

		TextureManagerVulkan::createImage(
			width,
			height,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			(*outTexture)->textureImage,
			(*outTexture)->textureImageMemory,
			1,
			renderDeviceVulkan->device
		);

		TextureManagerVulkan::createImageView(
			(*outTexture)->textureImage,
			(*outTexture)->textureImageView,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			1,
			renderDeviceVulkan->device
		);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // Clear to 1.0 depth

		TextureManagerVulkan::createTextureSampler(
			(*outTexture)->textureSampler,
			renderDeviceVulkan->device,
			samplerInfo
		);
	};

	createTexture(&momentImage);
	createTexture(&tempMomentImage);
}

void ShadowMapRendererVulkan::_createMomentDescriptor()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
	compDescriptorLayoutID = descriptorManagerVulkan->createLayout(bindings);

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 4 },
	};

	compDescriptorPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount * 2);

	compDescSetMtoT_ID = descriptorManagerVulkan->createSets(compDescriptorLayoutID, compDescriptorPoolID, 1);
	compDescSetTtoM_ID = descriptorManagerVulkan->createSets(compDescriptorLayoutID, compDescriptorPoolID, 1);
	auto setsMtoT = descriptorManagerVulkan->getDescriptorSet(compDescSetMtoT_ID);
	auto setsTtoM = descriptorManagerVulkan->getDescriptorSet(compDescSetTtoM_ID);

	for (uint32_t i = 0; i < 1; i++) {
		VkDescriptorImageInfo momentInfo{ momentImage->textureSampler, momentImage->textureImageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo tempInfo{ tempMomentImage->textureSampler, tempMomentImage->textureImageView, VK_IMAGE_LAYOUT_GENERAL };

		VkWriteDescriptorSet writesA[2] = {};
		// Binding 0: MomentImage
		writesA[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writesA[0].dstSet = setsMtoT[i];
		writesA[0].dstBinding = 0;
		writesA[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writesA[0].descriptorCount = 1;
		writesA[0].pImageInfo = &momentInfo;

		// Binding 1: TempMomentImage
		writesA[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writesA[1].dstSet = setsMtoT[i];
		writesA[1].dstBinding = 1;
		writesA[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writesA[1].descriptorCount = 1;
		writesA[1].pImageInfo = &tempInfo;
		vkUpdateDescriptorSets(renderDeviceVulkan->device, 2, writesA, 0, nullptr);

		VkWriteDescriptorSet writesB[2] = {};
		// Binding 0: MomentImage
		writesB[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writesB[0].dstSet = setsTtoM[i];
		writesB[0].dstBinding = 0;
		writesB[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writesB[0].descriptorCount = 1;
		writesB[0].pImageInfo = &tempInfo;

		// Binding 1: TempMomentImage
		writesB[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writesB[1].dstSet = setsTtoM[i];
		writesB[1].dstBinding = 1;
		writesB[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writesB[1].descriptorCount = 1;
		writesB[1].pImageInfo = &momentInfo;
		vkUpdateDescriptorSets(renderDeviceVulkan->device, 2, writesB, 0, nullptr);
	}
}

void ShadowMapRendererVulkan::_createOffscreenViewDescriptorSet()
{
	const uint32_t MAX_NUM_SETS = 3;	// add more if requires more
	
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

void ShadowMapRendererVulkan::_createComputePipeline() {
	computePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(compDescriptorLayoutID);

	std::vector<VkDescriptorSetLayout> layouts = { layout };

	computePipeline->createComputePipeline(
		"assets/shaders/shadowMap.comp.spv",
		layouts,
		sizeof(ComputePushConstant)
	);
}