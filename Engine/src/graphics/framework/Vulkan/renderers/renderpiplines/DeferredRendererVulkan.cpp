#include "DeferredRendererVulkan.h"
#include "DeferredRendererVulkan.h"
#include "DeferredRendererVulkan.h"
#include "DeferredRendererVulkan.h"
#include "DeferredRendererVulkan.h"
#include "DeferredRendererVulkan.h"
#include "DeferredRendererVulkan.h"
#include "DeferredRendererVulkan.h"

#include "core/features/ServiceLocator.h"
#include "graphics/renderers/RenderDevice.h"
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
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include <core/scene/SceneManager.h>

DeferredRendererVulkan::DeferredRendererVulkan()
{

}

DeferredRendererVulkan::~DeferredRendererVulkan()
{

}

bool DeferredRendererVulkan::init(WindowConfig config)
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

	_createRenderPasses();
	_createFrameBuffers();

	instanceData.reserve(numInstances);			// reserve the ssbo size
	instanceData.push_back({ glm::mat4(1.0) });	// prevent no entity size 0

	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));
	
	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}

	for(auto& entity : scene->getEntitiesWith<TransformComponent>()) {
		TransformComponent transform = entity.getComponent<TransformComponent>();
		instanceData.push_back({ transform.getModelMatrix() });
	}

	size_t bufferSize = instanceData.size() * sizeof(StorageBufferObject);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);
	
	_createDescriptor();
	_createPipelines();
	_createViewDescriptorSets();

	_createLightDescriptor();
	_createLightPipeline();
}

bool DeferredRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	gPassPipeline->destroy();
	renderTarget.destroy(renderDeviceVulkan->device);

	return true;
}

void DeferredRendererVulkan::onUpdate()
{
	
}

void DeferredRendererVulkan::beginFrame()
{
	renderDeviceVulkan->beginFrame();
}

void DeferredRendererVulkan::endFrame()
{
	renderDeviceVulkan->endFrame();
}

void DeferredRendererVulkan::render(Camera& camera)
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


	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	
	recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());
}

void DeferredRendererVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	beginRecording(
		commandBuffer,
		renderTarget.renderPass,
		renderTarget.framebuffers[imageIndex]
	);


	gPassPipeline->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport();
	renderDeviceVulkan->setScissor();

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		gPassPipeline->pipelineLayout,
		0,
		1,
		&descriptorSets[currentFrame],
		0,
		nullptr
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
				materialManager->bindMaterial(mesh->materialID, commandBuffer, (void*)gPassPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		} 
		
		if (entity.hasComponent<MeshComponent>()) {
			MeshComponent meshComponent = entity.getComponent<MeshComponent>();
			for (uint32_t meshID : meshComponent.meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, commandBuffer, (void*)gPassPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		}
		
		index++;
	}

	// --- PHASE 2: TRANSITION ---
    // This satisfies the "subpass index minus one" error
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

    // --- PHASE 3: SUBPASS 1 (Lighting) ---
    // 1. Bind the Lighting Pipeline
    lightingPipeline->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

    // 2. Bind the Lighting Descriptor Set (The G-Buffer Input Attachments)
    // You'll need a new setsID for the lighting pass descriptors
    auto lightSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        lightingPipeline->pipelineLayout,
        0, 1, &lightSets[currentFrame],
        0, nullptr
    );

    // 3. Draw the full-screen triangle (3 vertices, no vertex buffer needed)
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	endRecording(commandBuffer);
}

void DeferredRendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer)
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


	std::array<VkClearValue, 5> clearValues{};
	clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Main Color
	clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Position
	clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Normal
	clearValues[3].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Albedo
	clearValues[4].depthStencil = {1.0f, 0};           // Depth (0.0 to 1.0 range)

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	//basic draw commands
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void DeferredRendererVulkan::endRecording(void* cmdBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	vkCmdEndRenderPass(commandBuffer);
}

void DeferredRendererVulkan::_createRenderPasses()
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

	VkAttachmentDescription gBufferPos{};
	gBufferPos.format = VK_FORMAT_R16G16B16A16_SFLOAT;		// High precision for positions later move to depth reconstruction
	gBufferPos.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferPos.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferPos.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 	// Don't need this after the pass!
	gBufferPos.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferPos.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription gBufferNorm{};
	gBufferNorm.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gBufferNorm.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferNorm.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferNorm.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gBufferNorm.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferNorm.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription gBufferAlbedo{};
	gBufferAlbedo.format = VK_FORMAT_R8G8B8A8_UNORM;
	gBufferAlbedo.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferAlbedo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferAlbedo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gBufferAlbedo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferAlbedo.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// --- SUBPASS 0: G-Buffer Generation ---
	VkAttachmentReference gBufferReferences[] = {
		{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, // Position
		{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, // Normal
		{3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}  // Albedo
	};

	VkAttachmentReference depthReference = {4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass0{};
	subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass0.colorAttachmentCount = 3;
	subpass0.pColorAttachments = gBufferReferences;
	subpass0.pDepthStencilAttachment = &depthReference;

	// These tell the shader to use 'subpassInput' to read from the G-Buffer
	VkAttachmentReference inputReferences[] = {
		{1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
	};

	// --- SUBPASS 1: Lighting ---
	VkAttachmentReference colorAttachmentRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass1{};
	subpass1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass1.colorAttachmentCount = 1;
	subpass1.pColorAttachments = &colorAttachmentRef;
	subpass1.inputAttachmentCount = 3;
	subpass1.pInputAttachments = inputReferences;
	// We can still use the depth buffer for testing in subpass 1 if needed
	subpass1.pDepthStencilAttachment = &depthReference;

	// ZERO INITIALIZE or validation layer will complain
	std::array<VkSubpassDependency, 2> dependencies{};

	// 1. Wait for swapchain to be ready
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// 2. Wait for Subpass 0 (G-Buffer) to finish before Subpass 1 (Lighting) reads
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

	std::array<VkAttachmentDescription, 5> allAttachments = { 
		colorAttachment, gBufferPos, gBufferNorm, gBufferAlbedo, depthAttachment 
	};
	std::array<VkSubpassDescription, 2> subpasses = { subpass0, subpass1 };

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = allAttachments.size();
	renderPassInfo.pAttachments = allAttachments.data();
	renderPassInfo.subpassCount = subpasses.size();
	renderPassInfo.pSubpasses = subpasses.data();
	renderPassInfo.dependencyCount = dependencies.size();
	renderPassInfo.pDependencies = dependencies.data();

	if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderTarget.renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen render pass!");
	}

}

void DeferredRendererVulkan::_createFrameBuffers()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;
	
	renderTarget.colorTextures.resize(swapchain.swapChainImages.size());
	renderTarget.gBufferPos.resize(swapchain.swapChainImages.size());
	renderTarget.gBufferNorm.resize(swapchain.swapChainImages.size());
	renderTarget.gBufferAlbedo.resize(swapchain.swapChainImages.size());
	renderTarget.depthTextures.resize(swapchain.swapChainImages.size());
	renderTarget.framebuffers.resize(swapchain.swapChainImageViews.size());

	for(size_t i = 0; i < renderTarget.colorTextures.size(); i++) {
		auto createTexture = [&] (VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect) -> TextureVulkan* {
			uint32_t id = textureManager->createTexture();
			auto* texture = static_cast<TextureVulkan*>(textureManager->getTexture(id));

			TextureManagerVulkan::createImage(
				swapchain.swapChainExtent.width,
				swapchain.swapChainExtent.height,
				format,
				VK_IMAGE_TILING_OPTIMAL,
				usage,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				texture->textureImage,
				texture->textureImageMemory,
				renderDeviceVulkan->device
			);

			TextureManagerVulkan::createImageView(
				texture->textureImage,
				texture->textureImageView,
				format,
				aspect,
				renderDeviceVulkan->device
			);

			TextureManagerVulkan::createTextureSampler(
				texture->textureSampler, 
				renderDeviceVulkan->device
			);

			return texture;
		};

		// for the output image
		renderTarget.colorTextures[i] = createTexture(
			swapchain.swapChainImageFormat, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		//TODO: for g-buffer positions image
		renderTarget.gBufferPos[i] = createTexture(
			VK_FORMAT_R16G16B16A16_SFLOAT, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		//TODO: for g-buffer normals image
		renderTarget.gBufferNorm[i] = createTexture(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		// TODO: for g-buffer albedo image
		renderTarget.gBufferAlbedo[i] = createTexture(
			VK_FORMAT_R8G8B8A8_UNORM, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		// _createDepthResources(renderDeviceVulkan->device, *textureTarget.depthTextures[i]);

		std::array<VkImageView, 5> attachments = {
			renderTarget.colorTextures[i]->textureImageView,
			renderTarget.gBufferPos[i]->textureImageView,
			renderTarget.gBufferNorm[i]->textureImageView,
			renderTarget.gBufferAlbedo[i]->textureImageView,
			renderDeviceVulkan->swapchain.depthImageView	// textureTarget.depthTextures[i]->textureImageView
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
}

void DeferredRendererVulkan::_createDescriptor()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = { 
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	};
	layoutID = descriptorManagerVulkan->createLayout(bindings);
	
	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount },
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);

	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, VulkanUtils::numFrames());
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

	for (size_t i = 0; i < VulkanUtils::numFrames(); i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo ssboInfo{};
		ssboInfo.buffer = static_cast<VkBuffer>(*storagebuffersList[i]);
		ssboInfo.offset = 0;
		ssboInfo.range = VK_WHOLE_SIZE;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}

	std::vector<VkDescriptorSetLayoutBinding> samplerBindings = { 
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	};
	samplerLayoutID = descriptorManagerVulkan->createLayout(samplerBindings);

	std::vector<VkDescriptorPoolSize> samplerPoolSizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	};

	samplerPoolID = descriptorManagerVulkan->createPool(samplerPoolSizes, 1);

	samplerSetID = descriptorManagerVulkan->createSets(samplerLayoutID, samplerPoolID, 1);
	auto samplerSet = descriptorManagerVulkan->getDescriptorSet(samplerSetID);


}

void DeferredRendererVulkan::_createPipelines()
{
	PipelineConfigInfo gBufferConfig = VulkanPipeline::defaultPipelineConfigInfo(3);
	gBufferConfig.renderPass = renderTarget.renderPass;

	auto bindingDescription = VulkanDevice::VertexVulkan::getBindingDescription();
	auto attributeDescriptions = VulkanDevice::VertexVulkan::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// 3. Layout: G-Buffer descriptor layouts
	pushConstantData.flag = true;
	pushConstantData.color = glm::vec3(1.0f, 1.0f, 0.0f);
	pushConstantData.range = glm::vec3(1.0f, 1.0f, 1.0f);
	pushConstantData.data = 0.1f;

	VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
	VkDescriptorPool descriptorPool = descriptorManagerVulkan->getDescriptorPool(poolID);
	VkDescriptorSetLayout samplerLayout = descriptorManagerVulkan->getDescriptorLayout(samplerLayoutID);


	std::vector<VkDescriptorSetLayout> layouts = { descriptorSetLayout, samplerLayout };
	
	gPassPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	gPassPipeline->createGraphicsPipeline(
		"assets/shaders/gBuffer.vert.spv", 
		"assets/shaders/gBuffer.frag.spv", 
		gBufferConfig, 
		vertexInputInfo, 
		layouts, 
		0
	);
}

void DeferredRendererVulkan::_createViewDescriptorSets()
{
	const uint32_t MAX_NUM_SETS = 24;	// add more if requires more
	
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
		
	for(int i = 0; i < renderTarget.gBufferPos.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(imGuisetIDs[i])[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.gBufferPos[i]->textureImageView;
		imageInfo.sampler = renderTarget.gBufferPos[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
	
	for(int i = 0; i < renderTarget.gBufferAlbedo.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(imGuisetIDs[i])[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.gBufferAlbedo[i]->textureImageView;
		imageInfo.sampler = renderTarget.gBufferAlbedo[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}

	
	for(int i = 0; i < renderTarget.gBufferNorm.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(imGuisetIDs[i])[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.gBufferNorm[i]->textureImageView;
		imageInfo.sampler = renderTarget.gBufferNorm[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}

void DeferredRendererVulkan::_createLightPipeline()
{
	// 1. Prepare the Layouts (The one we made with 3 Input Attachments)
	auto lightDescriptorSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
	auto lightDescriptorLayout = descriptorManagerVulkan->getDescriptorLayout(lightLayoutID);

	std::vector<VkDescriptorSetLayout> lightLayouts = { lightDescriptorLayout };

	// 2. Prepare empty Vertex Input (No vertex buffer used)
	VkPipelineVertexInputStateCreateInfo emptyVertexInput{};
	emptyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	emptyVertexInput.vertexAttributeDescriptionCount = 0;
	emptyVertexInput.pVertexAttributeDescriptions = nullptr;
	emptyVertexInput.vertexBindingDescriptionCount = 0;
	emptyVertexInput.pVertexBindingDescriptions = nullptr;

	// 3. Configure the PipelineInfo (Subpass 1, 1 Attachment, No Depth)
	PipelineConfigInfo lightConfig = VulkanPipeline::defaultPipelineConfigInfo(1);
	lightConfig.renderPass = renderTarget.renderPass;
	lightConfig.subpass = 1; // CRITICAL: Target the second subpass
	lightConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
	lightConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

	// 4. Create it!
	lightingPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	lightingPipeline->createGraphicsPipeline(
		"assets/shaders/lightPass.vert.spv",
		"assets/shaders/lightPass.frag.spv",
		lightConfig,
		emptyVertexInput,
		lightLayouts,
		0
	);

}

void DeferredRendererVulkan::_createLightDescriptor()
{
    VkDevice device = renderDeviceVulkan->device;

    // 1. Layout
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    lightLayoutID = descriptorManagerVulkan->createLayout(bindings);
    
    // 2. Pool - Over-allocating to 100 to prevent any "tight fit" crashes
    uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100} 
    };
    // Create pool with maxSets = 100
    uint32_t lightPoolID = descriptorManagerVulkan->createPool(poolSizes, 100);

    // 3. Sets - Use the correct IDs here!
    lightSetsID = descriptorManagerVulkan->createSets(lightLayoutID, lightPoolID, frameCount);
    auto lightingDescriptorSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
    
    // 4. Update
    for(int i = 0; i < frameCount; i++) {
        VkDescriptorImageInfo posInfo = {VK_NULL_HANDLE, renderTarget.gBufferPos[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo normInfo = {VK_NULL_HANDLE, renderTarget.gBufferNorm[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo albedoInfo = {VK_NULL_HANDLE, renderTarget.gBufferAlbedo[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        std::array<VkWriteDescriptorSet, 3> writes{};
        
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = lightingDescriptorSets[i];
        writes[0].dstBinding = 0; // Position
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &posInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = lightingDescriptorSets[i];
        writes[1].dstBinding = 1; // Normal
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &normInfo;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = lightingDescriptorSets[i];
        writes[2].dstBinding = 2; // Albedo
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &albedoInfo;

        vkUpdateDescriptorSets(device, 3, writes.data(), 0, nullptr);
    }
}