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
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
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

	RendererVulkan* renderer = nullptr;
	renderer = rendererManagerVulkan->getRenderer("ShadowMapRendererVulkan");
	shadowMapRenderer = dynamic_cast<ShadowMapRendererVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
	
	assert(shadowMapRenderer && imageBasedRenderer && "failed to retrieve renderer");

	pushConstantLight.skyboxDetail = 0.0f;
	pushConstantLight.color = sunColor * sunIntensity;
	pushConstantLight.bias = 0.001f;
	pushConstantLight.alpha = 0.0001f;
    pushConstantLight.lintstepLow = 0.2f;
    pushConstantLight.linstepHigh = 1.0f;
    pushConstantLight.litBias = 0.0005f;

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

	RendererVulkan* tmp = rendererManagerVulkan->getRenderer("ShadowMapRendererVulkan");
	shadowMapRenderer = dynamic_cast<ShadowMapRendererVulkan*>(tmp);
	assert(shadowMapRenderer && "fail to retrieve shadowmap renderer");

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
	
}

void ForwardRendererVulkan::render(Camera& camera)
{
    Timer timer(m_name, true);

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

	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.cameraPos = glm::vec4(camera.getPosition(), 1.0);
	ubo.proj[1][1] *= -1;
	ubo.invView = camera.getInViewMatrix();
	ubo.invProj = camera.getInProjectionMatrix();
	ubo.invProj[1][1] *= -1;
	ubo.width = renderTarget.width;
	ubo.height = renderTarget.height;
	
	pushConstantLight.color = sunColor * sunIntensity;
	pushConstantLight.direction = glm::vec4(shadowMapRenderer->lightDir, 0.0f);
	pushConstantLight.sunlightMVP = shadowMapRenderer->lightSpaceMatrix;
    pushConstantLight.time = AppWindow::getTime();
	pushConstantLight.numLights = lights.size();

	uint32_t frame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[frame]->update(&ubo, sizeof(ubo));

	StorageBufferVulkan* ssbo = storagebuffersList[frame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	StorageBufferVulkan* lightSSBO = lightStoragebuffers[frame];
	lightSSBO->update(lights.data(), lights.size() * sizeof(LightSSBO));

	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	shadowMapRenderer->render(camera);
	imageBasedRenderer->onUpdate();
	imageBasedRenderer->computeSH(cmdBuffer, frame);
	imageBasedRenderer->computePrefilter(cmdBuffer, frame);

	recordDrawToTextureCommand(cmdBuffer, frame);
	rendererManagerVulkan->setDisplayImage(renderTarget.colorTextures[frame]);
}

void ForwardRendererVulkan::recordDrawToTextureCommand(VkCommandBuffer cmd, uint32_t imageIndex)
{
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
		sizeof(PushConstantLight),
		&pushConstantLight
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
		sizeof(PushConstantLight)
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
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	};
	
	layoutID = descriptorManagerVulkan->createLayout(bindings);
}

void ForwardRendererVulkan::_createDescriptorPool()
{
	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 3 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 5 }
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

		VkDescriptorBufferInfo lightsBufferInfo{};
		lightsBufferInfo.buffer = static_cast<VkBuffer>(*lightStoragebuffers[i]);
		lightsBufferInfo.offset = 0;
		lightsBufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = shadowMapRenderer->depthMap->textureImageView;
		imageInfo.sampler = shadowMapRenderer->depthMap->textureSampler;
		// imageInfo.imageView = shadowMapRenderer->momentImage->textureImageView;
		// imageInfo.sampler = shadowMapRenderer->momentImage->textureSampler;

		VkDescriptorImageInfo noiseImageInfo{};
		noiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		noiseImageInfo.imageView = shadowMapRenderer->blueNoiseImage->textureImageView;
		noiseImageInfo.sampler = shadowMapRenderer->blueNoiseImage->textureSampler;

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
 

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 2, lightsBufferInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 3, imageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 4, noiseImageInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 5, bufferInfoSH);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 6, brdfLutImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 7, prefilterImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 8, hdrImageInfo);
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