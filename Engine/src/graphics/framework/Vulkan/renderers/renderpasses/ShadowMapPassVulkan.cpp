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
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <core/scene/SceneManager.h>
#include <animation/AnimationManager.h>
#include "BlurPassVulkan.h"
#include "GeometryPassVulkan.h"
#include <graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/AccelStructureBufferVulkan.h>
#include <volk.h>

ShadowMapPassVulkan::ShadowMapPassVulkan() 
	: RendererVulkan("ShadowMapPassVulkan")
{
	setFootprint("ShadowMapPassVulkan", sizeof(ShadowMapPassVulkan), MemorySystem::Graphics);


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

	
	textureManagerVulkan->registerBindlessTexture(depthMap->id());
	textureManagerVulkan->registerBindlessTexture(blueNoiseImage->id());

	// the ray trace renderer is only added on gpus with ray tracing, without it there is no tlas to trace against
	rtSupported = rendererManagerVulkan->getRenderer("RayTraceRendererVulkan") != nullptr;
	blurPass = dynamic_cast<BlurPassVulkan*>(rendererManagerVulkan->getRenderer("BlurPassVulkan"));
	if(rtSupported) {
		_createRayTraced();
	}

	settings =&rendererManagerVulkan->getGraphicsSettings().add<ShadowMapSettings>("Shadow Map");

	return true;
}

bool ShadowMapPassVulkan::onClose()
{
	rendererManagerVulkan->getGraphicsSettings().remove<ShadowMapSettings>();
	settings = nullptr;

	shadowPipeline->destroy();
	vkDestroyFramebuffer(renderDeviceVulkan->device, shadowFramebuffer, nullptr);
	vkDestroyRenderPass(renderDeviceVulkan->device, shadowRenderPass, nullptr);

	computePipeline->destroy();
	if(rtPipeline) {
		rtPipeline->destroy();
	}

	return false;
}

void ShadowMapPassVulkan::onUpdate()
{
}

void ShadowMapPassVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);
	_applyResolution();

	glm::mat4 lightProjection;
	if(settings->useOrtho) {
		glm::mat4 rotElevation = glm::rotate(glm::mat4(1.0f), -settings->sunElevation, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 rotAzimuth = glm::rotate(glm::mat4(1.0f), settings->sunAzimuth, glm::vec3(0.0f, 1.0f, 0.0f));
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
		
		float s = settings->orthoExtent;
		lightProjection = glm::ortho(-s, s, -s, s, settings->zNear, settings->zFar);

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

	// row 2 of the view is the axis pointing from the scene back to the light, should be direction the shadow map is rendered from
	settings->lightDir = glm::normalize(glm::vec3(lightView[0][2], lightView[1][2], lightView[2][2]));

	lightProjection[1][1] *= -1;
	settings->lightSpaceMatrix = lightProjection * lightView;

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
	auto animationManager = &ServiceLocator::GetService<AnimationManager>("AnimationManager");

	LightPushConstant push{};
	push.lightMVP = settings->lightSpaceMatrix;
	push.objectsRef = rendererManagerVulkan->getObjectsAddress();
	push.boneMatricesRef = bufferManagerVulkan->getVirtualMemoryAddress(animationManager->getAnimationBufferAddressID(renderDeviceVulkan->getCurrentFrameIndex()));

	beginRecording(commandBuffer, shadowRenderPass, shadowFramebuffer, shadowPipeline.get());
	{
		auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			shadowPipeline->pipelineLayout,
			0,
			1,
			&bindlessSet,
			0,
			nullptr
		);

		vkCmdPushConstants(
			commandBuffer,
			shadowPipeline->pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(LightPushConstant),
			&push
		);

		rendererManagerVulkan->drawObjects(commandBuffer);
	}
	endRecording(commandBuffer);
}

void ShadowMapPassVulkan::dispatchBlur(VkCommandBuffer cmd, uint32_t frameIndex) 
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
	pushconstant.radius = settings->blurRadius;
	pushconstant.sigma = settings->blurSigma;
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

	TextureConfig imageConfig {
		.width = width,
		.height = height,
		.format = depthFormat,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		.aspectBits = VK_IMAGE_ASPECT_DEPTH_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	TextureSamplerConfig samplerConfig { .addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT };
	depthID = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
	depthMap = textureManagerVulkan->getTexture(depthID);
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

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(textureManagerVulkan->getBindlessTextureLayout());
	std::vector<VkDescriptorSetLayout> layouts = { bindlessLayout };	// alpha masked casters sample their albedo

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
	momentAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// or GENERAL for compute

	VkAttachmentReference momentReference{};
	momentReference.attachment = 0;
	momentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
    depthAttachment.format = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
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
	auto createTexture = [&](TextureVulkan*& texture) {
		TextureConfig imageConfig {
			.width = width,
			.height = height,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};
		TextureSamplerConfig samplerConfig {
			.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE	// clear to 1.0 depth
		};
		uint32_t imageID = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
		texture = textureManagerVulkan->getTexture(imageID);
	};

	createTexture(momentImage);
	createTexture(tempMomentImage);

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
	_writeMomentDescriptors();
}

// the blur ping-pongs moment -> temp -> moment, rewritten when the shadow map size changes
void ShadowMapPassVulkan::_writeMomentDescriptors()
{
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

void ShadowMapPassVulkan::_recreateResources()
{
	m_logger->warn("recource recreation unimlemented");
}

void ShadowMapPassVulkan::_cleanupResources()
{
	m_logger->warn("recource cleanup unimlemented");
}

void ShadowMapPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
	_applyResolution();
	auto depth = graph.importTexture(depthMap);

	RenderPassNode node{};
	node.name = "ShadowMap";
	node.writes = {};
	node.writes = { { depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };

	node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
		Camera* camera = SceneManager::cameraController;
		if(!camera) {
			return;
		}
		render(*camera);
	};

	graph.addPass(node);
}

void ShadowMapPassVulkan::_createRayTraced()
{
	rtBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 2 },
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frameCount },
	};
	rtLayoutID = descriptorManagerVulkan->createLayout(rtBindings);
	rtPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
	rtSetsID = descriptorManagerVulkan->createSets(rtLayoutID, rtPoolID, frameCount);

	VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(rtLayoutID);
	rtPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	rtPipeline->createComputePipeline("assets/shaders/spv/rtShadow.comp.spv", { layout }, sizeof(RayTracedPushConstant));
}

void ShadowMapPassVulkan::addRayTracedToGraph(RenderGraphVulkan& graph)
{
	m_rayTraced = false;
	if(!rtSupported || !rtPipeline || settings->method != 1) {
		return;
	}

	// built here instead of when the node runs so a scene without a tlas yet keeps the shadow map this frame
	auto raytracer = dynamic_cast<RayTraceRendererVulkan*>(rendererManagerVulkan->getRenderer("RayTraceRendererVulkan"));
	raytracer->updateTlas();
	rtTlas = raytracer->m_rtBuilder.getTlas();
	if(!rtTlas) {
		return;
	}

	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	TextureVulkan* positionImage = geometryPass->renderTarget.gBufferPos;
	TextureVulkan* normalImage = geometryPass->renderTarget.gBufferNorm;
	TextureVulkan* depthImage = geometryPass->renderTarget.depthTexture;

	RGTextureDesc desc{};
	desc.config.format = VK_FORMAT_R16_SFLOAT;
	desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	rtMaskHandle = graph.createRenderTexture("Shadow.RTMask", desc);
	rtMask = graph.getTexture(rtMaskHandle);

	auto position = graph.importTexture(positionImage);
	auto normal = graph.importTexture(normalImage);

	RenderPassNode node{};
	node.name = "RayTracedShadow";
	node.reads = {
		{ position, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
	};
	node.writes = { { rtMaskHandle, VK_IMAGE_LAYOUT_GENERAL } };
	node.execute = [this, positionImage, normalImage](VkCommandBuffer cmd, uint32_t currentFrame) {
		_writeRayTraced(cmd, currentFrame, positionImage, normalImage);
	};
	graph.addPass(node);

	rtMaskOutput = rtMask;
	if(settings->rtBlurRadius > 0 && blurPass) {
		BlurPassVulkan::BlurParams blurParams{};
		blurParams.type = BlurType::Bilateral;
		blurParams.depth = graph.importTexture(depthImage);
		if(Camera* camera = SceneManager::cameraController) {
			glm::mat4 proj = camera->getProjectionMatrix();
			proj[1][1] *= -1.0f;
			blurParams.invProj = glm::inverse(proj);
		}
		blurParams.radius = settings->rtBlurRadius;
		blurParams.sigma = static_cast<float>(settings->rtBlurRadius);
		blurParams.depthScale = 100.0f;
		rtMaskOutput = graph.getTexture(blurPass->blur(graph, "Shadow", rtMaskHandle, blurParams));
	}

	m_rayTraced = true;
}

void ShadowMapPassVulkan::_writeRayTraced(VkCommandBuffer cmd, uint32_t currentFrame, TextureVulkan* positionImage, TextureVulkan* normalImage)
{
	// the sun direction comes from the shadow map node earlier this frame, the tlas can be rebuilt between frames
	rtPush.sunDirection = glm::vec4(settings->lightDir, 0.0f);
	rtPush.coneAngle = glm::radians(std::max(settings->rtSoftness, 0.0f)) * 0.5f;
	rtPush.rayCount = std::clamp(settings->rtRayCount, 1, 64);
	rtPush.maxDistance = 1000.0f;

	auto sets = descriptorManagerVulkan->getDescriptorSet(rtSetsID);
	VkDescriptorImageInfo maskInfo{ rtMask->textureSampler, rtMask->textureImageView, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo positionInfo{ positionImage->textureSampler, positionImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo normalInfo{ normalImage->textureSampler, normalImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, sets[currentFrame], 0, maskInfo);
	descriptorManagerVulkan->writeImage(&writes, sets[currentFrame], 1, positionInfo);
	descriptorManagerVulkan->writeImage(&writes, sets[currentFrame], 2, normalInfo);
	descriptorManagerVulkan->writeAccelStruct(&writes, sets[currentFrame], 3, rtBindings, rtTlas->getDescAccelStructInfo());
	descriptorManagerVulkan->updateDescriptorSets(&writes);

	renderDeviceVulkan->beginLabel(cmd, "Ray Traced Shadow");
	rtPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
	vkCmdPushConstants(cmd, rtPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RayTracedPushConstant), &rtPush);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rtPipeline->pipelineLayout, 0, 1, &sets[currentFrame], 0, nullptr);
	vkCmdDispatch(cmd, (rtMask->width() + 15) / 16, (rtMask->height() + 15) / 16, 1);
	renderDeviceVulkan->endLabel(cmd);
}

// resolution comes from the settings, everything sized by it is rebuilt before this frame uses the shadow map
void ShadowMapPassVulkan::_applyResolution()
{
	static constexpr uint32_t sizes[] = { 512, 1024, 2048, 4096 };
	uint32_t size = sizes[std::clamp(settings->resolution, 0, 3)];
	if(size == width && size == height) {
		return;
	}

	renderDeviceVulkan->waitIdle();
	vkDestroyFramebuffer(renderDeviceVulkan->device, shadowFramebuffer, nullptr);
	textureManagerVulkan->destroy(depthID);
	textureManagerVulkan->destroy(momentImage->id());
	textureManagerVulkan->destroy(tempMomentImage->id());

	width = size;
	height = size;
	_createDepthMap();
	_createMomentImage();
	_writeMomentDescriptors();
	_createShadowFrameBuffer();
	textureManagerVulkan->registerBindlessTexture(depthMap->id());
}
