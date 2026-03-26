#include "ForwardRendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "core/events/EventManager.h"
#include "graphics/renderers/RenderDevice.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"

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
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include <core/scene/SceneManager.h>
#include <imgui.h>


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
		if (keyPressedEvent.keyCode == KEY_0) {
			pushConstantData.flag = !pushConstantData.flag;
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

	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));

	instanceData.resize(10000);
	size_t bufferSize = 10000 * sizeof(StorageBufferObject);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);

	lights.reserve(numLights);
	size_t lightBufferSize = numLights * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);


	_createDescriptorSets();
	_createOffscreenTarget();
	_createPipeline();

	return true;
}

bool ForwardRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();

	return true;
}

void ForwardRendererVulkan::onUpdate()
{
	render(*SceneManager::cameraController);
}

void ForwardRendererVulkan::render(Camera& camera)
{
	if (needResize) {
        _recreateResources();
        needResize = false;
        return; 
    }

	instanceData.clear(); 
    lights.clear();

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}
    auto entities = scene->getEntitiesWith<TransformComponent>();
    for (auto& entity : entities) {
        auto& transform = entity.getComponent<TransformComponent>();
        instanceData.push_back({ transform.getModelMatrix() });

        if (entity.hasComponent<LightComponent>()) {
            auto& light = entity.getComponent<LightComponent>();
			lights.push_back(LightSSBO(
				light.color, 
				glm::vec4(transform.translateVec, 1.0), 
				light.intensity * transform.scaleVec.x
			));
        }
    }

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
	// recordDrawToTextureCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());
	recordDrawToTextureCommand(cmdBuffer, frame);
	rendererManagerVulkan->setDisplayImage(renderTarget.colorTextures[frame]);
}

void ForwardRendererVulkan::recordDrawToTextureCommand(VkCommandBuffer cmd, uint32_t imageIndex)
{
	Timer timer("CPU render submission time", true);

	beginRecording(
		cmd,
		renderTarget.renderPass,
		renderTarget.framebuffers[imageIndex]
	);
	
	offscreenPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport(renderTarget.width, renderTarget.height);
	renderDeviceVulkan->setScissor(renderTarget.width, renderTarget.height);

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		offscreenPipeline->pipelineLayout,
		0,
		1,
		&descriptorSets[currentFrame],
		0,
		nullptr
	);

	vkCmdPushConstants(
		cmd,
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

	int index = 0;
	int lightIndex = 0;
	for (auto& entity : scene->getEntitiesWith<TransformComponent>()) {
		TransformComponent& transform = entity.getComponent<TransformComponent>();
		const glm::mat4& entityTransform = transform.getModelMatrix();
		glm::vec3& translation = transform.translateVec;
		
		if(index >= instanceData.size()) {
			instanceData.push_back({entityTransform});
			continue;
		}

		// TODO: copy the multiple all transforms to ssbo would be slow
		if (instanceData[index].model != entityTransform) {
			instanceData[index].model = entityTransform;
		}

		if (entity.hasComponent<LightComponent>()) {
			lightIndex++;
		}

		if(entity.hasComponent<ModelComponent>()) {
			uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
			const Model* model = modelManager->getModel(modelID);

			if (!model) {
				continue;
			}
			for (uint32_t meshID : model->meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, cmd, (void*)offscreenPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		} 
		
		if (entity.hasComponent<MeshComponent>()) {
			MeshComponent meshComponent = entity.getComponent<MeshComponent>();
			for (uint32_t meshID : meshComponent.meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, cmd, (void*)offscreenPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		}
		
		index++;
	}

	endRecording(cmd);
}

void ForwardRendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer)
{
	uint32_t imageIndex = renderDeviceVulkan->getImageIndex();
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);
	VkRenderPass vulkanRenderPass = static_cast<VkRenderPass>(renderPass);
	VkFramebuffer vulkanFrameBuffer = static_cast<VkFramebuffer>(frameBuffer);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vulkanRenderPass;
	renderPassInfo.framebuffer = vulkanFrameBuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = { renderTarget.width, renderTarget.height };


	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.15f, 0.15f, 0.15f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}


void ForwardRendererVulkan::endRecording(void* cmdBuffer)
{
	vkCmdEndRenderPass(static_cast<VkCommandBuffer>(cmdBuffer));
}

void ForwardRendererVulkan::_createPipeline()
{
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

void ForwardRendererVulkan::_createOffscreenTarget()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;
	renderTarget.width = swapchain.swapChainExtent.width;
	renderTarget.height = swapchain.swapChainExtent.height;

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

	VkSubpassDependency dependency2{};
	dependency2.srcSubpass = 0;
	dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
	dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	std::array<VkSubpassDependency, 2> dependencies = { dependency, dependency2 };
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = dependencies.size();
	renderPassInfo.pDependencies = dependencies.data();

	if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderTarget.renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen render pass!");
	}

	uint32_t numFrames = VulkanUtils::numFrames();
	renderTarget.colorTextures.resize(numFrames);
	renderTarget.depthTextures.resize(numFrames);
	renderTarget.framebuffers.resize(numFrames);

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

			
			VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
			TextureManagerVulkan::transitionImageLayout(
				cmd,
				texture->textureImage,
				swapchain.swapChainImageFormat,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				1,
				1,
				renderDeviceVulkan
			);
			
			renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

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
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
		framebufferInfo.width = renderDeviceVulkan->swapchain.swapChainExtent.width;
		framebufferInfo.height = renderDeviceVulkan->swapchain.swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &renderTarget.framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create offscreen framebuffer!");
		}
	}
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

void ForwardRendererVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	rendererManagerVulkan->setDisplayImage(nullptr);
	_cleanupResources();

	_createOffscreenTarget();
	_updateDescriptor();
	_createPipeline();
}

void ForwardRendererVulkan::_cleanupResources()
{
	renderTarget.destroy(renderDeviceVulkan->device);
	offscreenPipeline->destroy();
}