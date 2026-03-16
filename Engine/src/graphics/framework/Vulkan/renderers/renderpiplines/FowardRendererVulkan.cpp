#include "ForwardRendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "window/AppWindow.h"
#include "graphics/renderers/RenderDevice.h"
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include "logging/Logger.h"
#include "core/events/EventManager.h"

#include <core/resources/managers/TextureManager.h>
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
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <core/scene/SceneManager.h>
#include "imgui.h" // TODO: remove it once done

ForwardRendererVulkan::ForwardRendererVulkan(std::string serviceName) 
	:	RendererVulkan(serviceName)
{

}

ForwardRendererVulkan::~ForwardRendererVulkan()
{

}

bool ForwardRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	EventManager::getInstance().subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_2) {
			showGui = !showGui;
		}
	});

	pushConstantData.flag = true;
	pushConstantData.color = glm::vec3(1.0f, 1.0f, 0.0f);
	pushConstantData.range = glm::vec3(1.0f, 1.0f, 1.0f);
	pushConstantData.data = 0.1f;

	_createDescriptorSetLayout();
	descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);

	_createDescriptorPool();
	descriptorPool = descriptorManagerVulkan->getDescriptorPool(poolID);
	
	void* handle = materialManager->getMaterialLayout();
	VkDescriptorSetLayout materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

	renderDeviceVulkan->pipeline.create();
	renderDeviceVulkan->pipeline.createGraphicsPipeline(
		"assets/shaders/spv/forwardLightPass.vert.spv",
		"assets/shaders/spv/forwardLightPass.frag.spv",
		{ descriptorSetLayout, materialLayout }, 
		renderDeviceVulkan->swapchain.renderPass, 
		sizeof(PushConstantData)
	);
	

	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));

	instanceData.resize(numInstances);					// reserve the ssbo size
	instanceData.push_back({ glm::mat4(1.0) });	// prevent no entity size 0
	size_t bufferSize = instanceData.size() * sizeof(StorageBufferObject);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);
	

	lights.reserve(numLights);
	lights.push_back(LightSSBO{glm::vec4(1.0, 1, 1.0, 1.0), 0, 1.0});
	lights.push_back(LightSSBO(glm::vec4(1.0, 1.0, 0.0, 1.0), 1, 5.5));
	size_t lightBufferSize = lights.size() * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);

	_createDescriptorSets();
	_createOffscreenTarget();

	EventManager::getInstance().subscribe(EventType::WindowResize, [this] (Event& event) {
		_recreteResources();
	});

	return true;
}

bool ForwardRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	renderDeviceVulkan->pipeline.destroy();

	offscreenPipeline->destroy();
	renderTarget.destroy(renderDeviceVulkan->device);

	return true;
}

void ForwardRendererVulkan::onUpdate()
{
	render(*SceneManager::cameraController);
}

void ForwardRendererVulkan::render(Camera& camera)
{
	UniformBufferObject ubo{};
	ubo.model = glm::scale(glm::mat4(1.0), glm::vec3(0.5f, 0.5f, 0.5f));
	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.proj[1][1] *= -1;

	uint32_t frame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[frame]->update(&ubo, sizeof(ubo));

	StorageBufferVulkan* ssbo = storagebuffersList[frame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	StorageBufferVulkan* lightSSBO = lightStoragebuffers[frame];
	lightSSBO->update(lights.data(), lights.size() * sizeof(LightSSBO));


	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	rendererManagerVulkan->setDisplayImage(renderTarget.colorTextures[currentFrame]);
	recordDrawToTextureCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());
}

void ForwardRendererVulkan::recordDrawToTextureCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	Timer timer("CPU render submission time", true);

	beginRecording(
		commandBuffer,
		renderTarget.renderPass,
		renderTarget.framebuffers[imageIndex],
		offscreenPipeline.get()
	);

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		offscreenPipeline->pipelineLayout,
		0,
		1,
		&descriptorSets[currentFrame],
		0,
		nullptr
	);

	vkCmdPushConstants(
		commandBuffer,
		offscreenPipeline->pipelineLayout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(PushConstantData),
		&pushConstantData
	);


	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if (!scene) {
		m_logger->error("No scene to render");
	}

	int index = 1;
	for (auto& entity : scene->getEntitiesWith<TransformComponent>()) {
		const glm::mat4& entityTransform = entity.getComponent<TransformComponent>().getModelMatrix();
		// TODO: copy the multiple all transforms to ssbo would be slow
		if(index < instanceData.size()) {
			if (instanceData[index].model != entityTransform) {
				instanceData[index].model = entityTransform;
			}
		} else {
			instanceData.push_back({ entityTransform });
		}
		

		if(entity.hasComponent<ModelComponent>()) {
			uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
			const Model* model = modelManager->getModel(modelID);

			for (uint32_t meshID : model->meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, commandBuffer, (void*)offscreenPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		}

		if (entity.hasComponent<MeshComponent>()) {
			MeshComponent meshComponent = entity.getComponent<MeshComponent>();
			for (uint32_t meshID : meshComponent.meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, commandBuffer, (void*)offscreenPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		}
		
		index++;
	}

	endRecording(commandBuffer);
}

void ForwardRendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer, void* pipeline)
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
	renderPassInfo.renderArea.extent = {AppWindow::getWidth(), AppWindow::getHeight()};


	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.15f, 0.15f, 0.15f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	//basic draw commands
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	pipelinePtr->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport();
	renderDeviceVulkan->setScissor();
}


void ForwardRendererVulkan::endRecording(void* cmdBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	vkCmdEndRenderPass(commandBuffer);
}

void ForwardRendererVulkan::_createOffscreenTarget()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain.swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	// Add a dependency to ensure the texture is ready before the main pass reads it
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderTarget.renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen render pass!");
	}

	renderTarget.colorTextures.resize(swapchain.swapChainImages.size());
	renderTarget.depthTextures.resize(swapchain.swapChainImages.size());
	renderTarget.framebuffers.resize(swapchain.swapChainImageViews.size());

	for(size_t i = 0; i < renderTarget.colorTextures.size(); i++) {
		uint32_t id = textureManagerVulkan->createTexture();

		auto createTexture = [&] () {
			auto* texture = static_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
			renderTarget.colorTextures[i] = texture;

			TextureManagerVulkan::createImage(
				swapchain.swapChainExtent.width,
				swapchain.swapChainExtent.height,
				swapchain.swapChainImageFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				texture->textureImage,
				texture->textureImageMemory,
				1,
				renderDeviceVulkan->device
			);

			TextureManagerVulkan::createImageView(
				texture->textureImage,
				texture->textureImageView,
				swapchain.swapChainImageFormat,
				VK_IMAGE_ASPECT_COLOR_BIT,
				1,
				renderDeviceVulkan->device
			);

			TextureManagerVulkan::createTextureSampler(
				texture->textureSampler, 
				renderDeviceVulkan->device
			);
		};

		createTexture();


		VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

		uint32_t depthId = textureManagerVulkan->createTexture();
		renderTarget.depthTextures[i] = static_cast<TextureVulkan*>(textureManagerVulkan->getTexture(depthId));

		TextureManagerVulkan::createImage(
			swapchain.swapChainExtent.width,
			swapchain.swapChainExtent.height,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			renderTarget.depthTextures[i]->textureImage,
			renderTarget.depthTextures[i]->textureImageMemory,
			1,
			renderDeviceVulkan->device
		);

		TextureManagerVulkan::createImageView(
			renderTarget.depthTextures[i]->textureImage,
			renderTarget.depthTextures[i]->textureImageView,
			depthFormat,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			1,
			renderDeviceVulkan->device
		);


		std::array<VkImageView, 2> attachments = {
			renderTarget.colorTextures[i]->textureImageView,
			renderTarget.depthTextures[i]->textureImageView
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderTarget.renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = AppWindow::getWidth();
		framebufferInfo.height = AppWindow::getHeight();
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &renderTarget.framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create offscreen framebuffer!");
		}
	}

	void* handle = materialManager->getMaterialLayout();
	VkDescriptorSetLayout materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

	offscreenPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	offscreenPipeline->createGraphicsPipeline(
		"assets/shaders/spv/forwardLightPass.vert.spv",
		"assets/shaders/spv/forwardLightPass.frag.spv",
		{ descriptorSetLayout, materialLayout }, 
		renderTarget.renderPass, 
		sizeof(PushConstantData)
	);
}

void ForwardRendererVulkan::_createDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = { 
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	};
	
	layoutID = descriptorManagerVulkan->createLayout(bindings);
}

void ForwardRendererVulkan::_createDescriptorPool()
{
	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 2 },
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
}

void ForwardRendererVulkan::_createDescriptorSets()
{
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, VulkanUtils::numFrames());
	descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

	_updateDescriptor();
}

void ForwardRendererVulkan::_updateDescriptor()
{
	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo ssboInfo{};
		ssboInfo.buffer = static_cast<VkBuffer>(*storagebuffersList[i]);
		ssboInfo.offset = 0;
		ssboInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo lightSsboInfo{};
		lightSsboInfo.buffer = static_cast<VkBuffer>(*lightStoragebuffers[i]);
		lightSsboInfo.offset = 0;
		lightSsboInfo.range = VK_WHOLE_SIZE;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 2, lightSsboInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}

void ForwardRendererVulkan::_recreteResources()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();
	_createOffscreenTarget();
	_updateDescriptor();
}

void ForwardRendererVulkan::_cleanupResources()
{
	renderTarget.destroy(renderDeviceVulkan->device);
}