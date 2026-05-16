#include "ShadowMapPassVulkan.h"
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

ShadowMapPassVulkan::ShadowMapPassVulkan() 
	: RendererVulkan("ShadowMapPassVulkan")
{


}

ShadowMapPassVulkan::~ShadowMapPassVulkan()
{
}

bool ShadowMapPassVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	uint32_t imageID = textureManagerVulkan->loadTexture("assets/textures/obluenoise256.png", 1, true);
	blueNoiseImage = textureManagerVulkan->getTexture(imageID);

	_createDepthMap();
	_createShadowRenderPass();
	_createShadowPipeline();

	_createMomentImage();
	_createMomentDescriptor();
	_createShadowFrameBuffer();
	_createComputePipeline();

	pushconstant.radius = 64;  	// range 1 to 64
    pushconstant.sigma = 15;  	// range 1.0 to 30.0
	lightDir = glm::normalize(glm::vec3(1.0f));
	s = 25.0f; 
	zNear = 0.01f;
	zFar = 105.0f;
	sunElevation = 1.357;

	return true;
}

bool ShadowMapPassVulkan::onClose()
{
	shadowPipeline->destroy();
	vkDestroyFramebuffer(renderDeviceVulkan->device, shadowFramebuffer, nullptr);
	vkDestroyRenderPass(renderDeviceVulkan->device, shadowRenderPass, nullptr);

	computePipeline->destroy();

	
	return false;
}

void ShadowMapPassVulkan::onUpdate()
{
}

void ShadowMapPassVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);

	glm::mat4 lightProjection;
	if(useOrtho) {
		glm::mat4 rotElevation = glm::rotate(glm::mat4(1.0f), -sunElevation, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 rotAzimuth = glm::rotate(glm::mat4(1.0f), sunAzimuth, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 sunRotation = rotAzimuth * rotElevation;

		lightPos = glm::vec3(sunRotation * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)) * 100.0f;

		glm::vec3 right   = glm::vec3(sunRotation[0]); 
		glm::vec3 up      = glm::vec3(sunRotation[1]);
		glm::vec3 forward = glm::vec3(sunRotation[2]);

		// manually construct the view / lookAt without flip logic
		lightView = glm::mat4(1.0f);
		lightView[0][0] = right.x;   lightView[1][0] = right.y;   lightView[2][0] = right.z;
		lightView[0][1] = up.x;      lightView[1][1] = up.y;      lightView[2][1] = up.z;
		lightView[0][2] = forward.x; lightView[1][2] = forward.y; lightView[2][2] = forward.z;
		lightView[3][0] = -glm::dot(right, lightPos);
		lightView[3][1] = -glm::dot(up, lightPos);
		lightView[3][2] = -glm::dot(forward, lightPos);
		
		lightProjection = glm::ortho(-s, s, -s, s, zNear, zFar);

		// glm::vec3 followTarget = camera.getPosition();
		// float s = 25.0f;
		// float zNear = 0.01f;
		// float zFar = 105.0f;
		// float worldUnitsPerTexel = (2.0f * s) / width;
		// followTarget.x = std::floor(followTarget.x / worldUnitsPerTexel) * worldUnitsPerTexel;
		// followTarget.y = std::floor(followTarget.y / worldUnitsPerTexel) * worldUnitsPerTexel;
		// followTarget.z = std::floor(followTarget.z / worldUnitsPerTexel) * worldUnitsPerTexel;
		// lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
		// lightPos = followTarget + (lightDir * 100.0f);
		// lightView = glm::lookAt(lightPos, followTarget, glm::vec3(0.0f, 1.0f, 0.0f));
		// glm::mat4 lightProjection = glm::ortho(-s, s, -s, s, zNear, zFar);
	} else {
		lightPos = glm::vec3(5.0f);
		glm::vec3 orientation = glm::vec3(-5.0f);
		lightView = glm::lookAt(lightPos, lightPos + orientation, glm::vec3(0.0f, 1.0f, 0.0f));
		if(height > 0){
			lightProjection = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 25.0f);
		}
	}

	lightProjection[1][1] *= -1;
	lightSpaceMatrix = lightProjection * lightView;

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    renderDeviceVulkan->beginLabel(cmd, "Shadow Pass");
	recordDrawCommand(cmd, renderDeviceVulkan->getImageIndex());
	dispatchBlur(cmd, renderDeviceVulkan->getImageIndex());
    renderDeviceVulkan->endLabel(cmd);
}

void ShadowMapPassVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer, void* pipeline)
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

void ShadowMapPassVulkan::endRecording(void* cmdBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	vkCmdEndRenderPass(commandBuffer);
}

void ShadowMapPassVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();

	beginRecording(commandBuffer, shadowRenderPass, shadowFramebuffer, shadowPipeline.get());
	{
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

		for (auto& entity : scene->getEntitiesWith<TransformComponent, MeshComponent>()) {
			auto& transform = entity.getComponent<TransformComponent>();
			auto& meshComp = entity.getComponent<MeshComponent>();

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
			
			for (uint32_t meshID : meshComp.meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
			}
		}	
	}
	endRecording(commandBuffer);
}

void ShadowMapPassVulkan::dispatchBlur(VkCommandBuffer cmd, uint32_t frameIndex) 
{
	// TextureManagerVulkan::transitionImageLayout(
		// cmd, momentImage->textureImage, VK_FORMAT_R32G32B32A32_SFLOAT,
		// VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan);
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
	pushconstant.isVertical = 0;
	vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstant), &pushconstant);

	auto& setsH = descriptorManagerVulkan->getDescriptorSet(compDescSetMtoT_ID);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &setsH[0], 0, nullptr);

	uint32_t groupsX = (width + 127) / 128;
	uint32_t groupsY = height; 
	vkCmdDispatch(cmd, groupsX, groupsY, 1);

	// wait for Horizontal to finish before Vertical starts
	TextureManagerVulkan::createBarrier(cmd, tempMomentImage->textureImage,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	// verticle pass tmp -> moment
	pushconstant.isVertical = 1;
	vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstant), &pushconstant);

	auto& setsV = descriptorManagerVulkan->getDescriptorSet(compDescSetTtoM_ID);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &setsV[0], 0, nullptr);

	uint32_t groupsAlongHeight = (height + 127) / 128;
    vkCmdDispatch(cmd, width, groupsAlongHeight, 1);

	TextureManagerVulkan::createBarrier(cmd, momentImage->textureImage,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}


void ShadowMapPassVulkan::_createDepthMap()
{
	VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

	depthID = textureManagerVulkan->createTexture();
	depthMap = static_cast<TextureVulkan*>(textureManagerVulkan->getTexture(depthID));

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

void ShadowMapPassVulkan::_createShadowPipeline()
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
	pipelineConfig.rasterizationInfo.depthBiasConstantFactor = 0.0f;
	pipelineConfig.rasterizationInfo.depthBiasSlopeFactor = 0.0f;
	
	// pipelineConfig.rasterizationInfo.depthBiasConstantFactor = 1.25f;
	// pipelineConfig.rasterizationInfo.depthBiasSlopeFactor = 1.75f;

	//pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
	// pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	// pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
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
		"assets/shaders/spv/shadowMap.vert.spv",
		"assets/shaders/spv/shadowMap.frag.spv",
		pipelineConfig,
		vertexInputInfo,
		layouts,
		sizeof(LightPushConstant)
	);
}

void ShadowMapPassVulkan::_createShadowRenderPass()
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

void ShadowMapPassVulkan::_createShadowFrameBuffer()
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

void ShadowMapPassVulkan::_createMomentImage()
{
	auto createTexture = [&](TextureVulkan** outTexture) {
		uint32_t imageID = textureManagerVulkan->createTexture();
		*outTexture = static_cast<TextureVulkan*>(textureManagerVulkan->getTexture(imageID));

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

void ShadowMapPassVulkan::_createMomentDescriptor()
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

void ShadowMapPassVulkan::_createComputePipeline() {
	computePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(compDescriptorLayoutID);

	std::vector<VkDescriptorSetLayout> layouts = { layout };

	computePipeline->createComputePipeline(
		"assets/shaders/spv/shadowMap.comp.spv",
		layouts,
		sizeof(ComputePushConstant)
	);
}