#include "RendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "graphics/renderers/RenderDevice.h"
#include "RenderDeviceVulkan.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"
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
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <core/scene/SceneManager.h>


#include "imgui.h" // TODO: remove it once done

RendererVulkan::RendererVulkan(std::string serviceName) 
	:	Renderer(serviceName)
{

}

RendererVulkan::~RendererVulkan()
{

}

bool RendererVulkan::init(WindowConfig config)
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

	if(!(
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

	EventManager::getInstance().subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_1) {
			pushConstantData.flag = !pushConstantData.flag;
		}
	});

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
		{ descriptorSetLayout, materialLayout }, 
		renderDeviceVulkan->swapchain.renderPass, 
		sizeof(PushConstantData)
	);
	

	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));


	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}
	
	instanceData.reserve(numInstances);			// reserve the ssbo size
	instanceData.push_back({ glm::mat4(1.0) });	// prevent no entity size 0

	for(auto& entity : scene->getEntitiesWith<TransformComponent>()) {
		TransformComponent transform = entity.getComponent<TransformComponent>();
		instanceData.push_back({ transform.getModelMatrix() });
	}

	size_t bufferSize = instanceData.size() * sizeof(StorageBufferObject);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);
	

	lights.reserve(numLights);
	lights.push_back(LightSSBO{glm::vec4(1.0, 1, 1.0, 1.0), 0, 1.0});
	lights.push_back(LightSSBO(glm::vec4(1.0, 1.0, 0.0, 1.0), 1, 5.5));
	size_t lightBufferSize = lights.size() * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);

	_createDescriptorSets();

	_createOffscreenTarget();
	_createOffscreenViewDescriptorSet();
	
	return true;
}

bool RendererVulkan::onClose()
{

	renderDeviceVulkan->waitIdle();
	renderDeviceVulkan->pipeline.destroy();

	offscreenPipeline->destroy();
	renderTarget.destroy(renderDeviceVulkan->device);

	return true;
}

void RendererVulkan::onUpdate()
{
	render(*SceneManager::cameraController);
}


void RendererVulkan::beginFrame()
{
	renderDeviceVulkan->beginFrame();
}

void RendererVulkan::endFrame()
{
	renderDeviceVulkan->endFrame();
}

void RendererVulkan::render(Camera& camera)
{
	UniformBufferObject ubo{};
	ubo.model = glm::mat4(1.0);
	ubo.model = glm::scale(ubo.model, glm::vec3(0.5f, 0.5f, 0.5f));
	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.proj[1][1] *= -1;

	uint32_t frame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[frame]->update(&ubo, sizeof(ubo));

	StorageBufferVulkan* ssbo = storagebuffersList[frame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	StorageBufferVulkan* lightSSBO = lightStoragebuffers[frame];
	lightSSBO->update(lights.data(), lights.size() * sizeof(LightSSBO));

	beginFrame();

	renderDeviceVulkan->render();
	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	
	
	renderDeviceVulkan->commandPool.beginBuffer();
	if(showGui) {
		recordDrawToTextureCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());
	}
	recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());
	renderDeviceVulkan->commandPool.endBuffer();
	
	endFrame();
}


void RendererVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	Timer("CPU render submission time", true);

	beginRecording(
		commandBuffer,
		renderDeviceVulkan->swapchain.renderPass,
		renderDeviceVulkan->swapchain.currentFrameBuffer(),
		&renderDeviceVulkan->pipeline
	);

	if(showGui){
		renderGui(commandBuffer);
	} 
	else {
		uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			renderDeviceVulkan->pipeline.pipelineLayout,
			0,
			1,
			&descriptorSets[currentFrame],
			0,
			nullptr
		);

		vkCmdPushConstants(
			commandBuffer,
			renderDeviceVulkan->pipeline.pipelineLayout,
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
			if (instanceData[index].model != entityTransform) {
				instanceData[index].model = entityTransform;
			}

			if(entity.hasComponent<ModelComponent>()) {
				uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
				const Model* model = modelManager->getModel(modelID);

				for (uint32_t meshID : model->meshIDs) {
					const Mesh* mesh = meshManager->getMesh(meshID);
					materialManager->bindMaterial(mesh->materialID, commandBuffer);
					meshManager->bindMesh(meshID);

					uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
					renderDeviceVulkan->draw(indexCount, numInstances, index);
				}
			} 
			
			if (entity.hasComponent<MeshComponent>()) {
				MeshComponent meshComponent = entity.getComponent<MeshComponent>();
				for (uint32_t meshID : meshComponent.meshIDs) {
					const Mesh* mesh = meshManager->getMesh(meshID);
					materialManager->bindMaterial(mesh->materialID, commandBuffer);
					meshManager->bindMesh(meshID);

					uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
					renderDeviceVulkan->draw(indexCount, numInstances, index);
				}
			}
			
			index++;
		}
	}

	endRecording(commandBuffer);
}

void RendererVulkan::recordDrawToTextureCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	Timer("CPU render submission time", true);

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
		if (instanceData[index].model != entityTransform) {
			instanceData[index].model = entityTransform;
		}

		if(entity.hasComponent<ModelComponent>()) {
			uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
			const Model* model = modelManager->getModel(modelID);

			for (uint32_t meshID : model->meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, commandBuffer);
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		} 
		
		if (entity.hasComponent<MeshComponent>()) {
			MeshComponent meshComponent = entity.getComponent<MeshComponent>();
			for (uint32_t meshID : meshComponent.meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, commandBuffer);
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		}
		
		index++;
	}

	endRecording(commandBuffer);
}

void RendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer, void* pipeline)
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
	renderPassInfo.renderArea.extent = renderDeviceVulkan->swapchain.swapChainExtent;


	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.15f, 0.15f, 0.15f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	//basic draw commands
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	offscreenPipeline->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport();
	renderDeviceVulkan->setScissor();
}


void RendererVulkan::endRecording(void* cmdBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	vkCmdEndRenderPass(commandBuffer);

}

void RendererVulkan::renderGui(void* commandBuffer)
{
	guiManager->start();
	ImGui::Begin("Application");
	ImGui::BeginChild("Application View");
	uint32_t imageIndex = renderDeviceVulkan->getImageIndex();
	VkDescriptorSet descSet = descriptorManagerVulkan->getDescriptorSet(imGuisetIDs[imageIndex])[0];
	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	uint32_t i = renderDeviceVulkan->getImageIndex();
	ImGui::Image(reinterpret_cast<ImTextureID>(descSet), viewportPanelSize);
	ImGui::EndChild();
	ImGui::End();
	guiManager->render(commandBuffer);
	guiManager->end();
}

void RendererVulkan::_createOffscreenTarget()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain.swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  	// Clear the texture before drawing
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Save the results
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // transitions the image for use in the NEXT shader automatically

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

	vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderTarget.renderPass);

	renderTarget.colorTextures.resize(swapchain.swapChainImages.size());
	renderTarget.depthTextures.resize(swapchain.swapChainImages.size());
	renderTarget.framebuffers.resize(swapchain.swapChainImageViews.size());

	for(size_t i = 0; i < renderTarget.colorTextures.size(); i++) {
		uint32_t id = textureManager->createTexture();
		auto* texture = static_cast<TextureVulkan*>(textureManager->getTexture(id));
		renderTarget.colorTextures[i] = texture;

		TextureManagerVulkan::createImage(
			swapchain.swapChainExtent.width,
			swapchain.swapChainExtent.height,
			// swapchain.swapChainImageFormat,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			texture->textureImage,
			texture->textureImageMemory,
			renderDeviceVulkan->device
		);

		TextureManagerVulkan::createImageView(
			texture->textureImage,
			texture->textureImageView,
			swapchain.swapChainImageFormat,
			VK_IMAGE_ASPECT_COLOR_BIT,
			renderDeviceVulkan->device
		);

		TextureManagerVulkan::createTextureSampler(
			texture->textureSampler, 
			renderDeviceVulkan->device
		);

		// _createDepthResources(renderDeviceVulkan->device, *textureTarget.depthTextures[i]);

		std::array<VkImageView, 2> attachments = {
			renderTarget.colorTextures[i]->textureImageView,
			// textureTarget.depthTextures[i]->textureImageView
			renderDeviceVulkan->swapchain.depthImageView
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderTarget.renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapchain.swapChainExtent.width;
		framebufferInfo.height = swapchain.swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &renderTarget.framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create offscreen framebuffer!");
		}
	}

	void* handle = materialManager->getMaterialLayout();
	VkDescriptorSetLayout materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

	offscreenPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	offscreenPipeline->createGraphicsPipeline(
		{ descriptorSetLayout, materialLayout }, 
		renderTarget.renderPass, 
		sizeof(PushConstantData)
	);
}

void RendererVulkan::_createDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = { 
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	};
	
	layoutID = descriptorManagerVulkan->createLayout(bindings);
}

void RendererVulkan::_createDescriptorPool()
{
	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 2 },
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
}

void RendererVulkan::_createDescriptorSets()
{
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, VulkanUtils::numFrames());
	descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

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

void RendererVulkan::_createOffscreenViewDescriptorSet()
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

	for(int i = 0; i < renderTarget.colorTextures.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(imGuisetIDs[i])[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.colorTextures[i]->textureImageView;
		imageInfo.sampler = renderTarget.colorTextures[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}

void RendererVulkan::_createDepthResources(VulkanDevice& device, TextureVulkan& depthTexture)
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(device);

	TextureManagerVulkan::createImage(
		swapchain.swapChainExtent.width,
		swapchain.swapChainExtent.height,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depthTexture.textureImage,
		depthTexture.textureImageMemory,
		device
	);

	TextureManagerVulkan::createImageView(depthTexture.textureImage, depthTexture.textureImageView, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, device);
}
