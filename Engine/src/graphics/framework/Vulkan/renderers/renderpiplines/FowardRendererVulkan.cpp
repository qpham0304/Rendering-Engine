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
	renderer = rendererManagerVulkan->getRenderer("ShadowMapPassVulkan");
	shadowMapRenderer = dynamic_cast<ShadowMapPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
	
	assert(shadowMapRenderer && imageBasedRenderer && "failed to retrieve renderer");

	pushConstantLight.skyboxDetail = 0.0f;
	pushConstantLight.color = sunColor * sunIntensity;
	pushConstantLight.bias = 0.001f;

	_createDescriptorSetLayout();
	descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);

	_createDescriptorPool();
	descriptorPool = descriptorManagerVulkan->getDescriptorPool(poolID);
	
	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));

	instanceData.resize(10000);
	size_t bufferSize = 10000 * sizeof(StorageBufferObject);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);

	lights.reserve(numLights);
	size_t lightBufferSize = numLights * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);

	RendererVulkan* tmp = rendererManagerVulkan->getRenderer("ShadowMapPassVulkan");
	shadowMapRenderer = dynamic_cast<ShadowMapPassVulkan*>(tmp);
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

    RendererVulkan::_resize();


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

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "shadow Pass");
	shadowMapRenderer->render(camera);
	renderDeviceVulkan->endLabel(cmd);

	renderDeviceVulkan->beginLabel(cmd, "IBL Pass");
	imageBasedRenderer->onUpdate();
	imageBasedRenderer->computeSH(cmd, frame);	//TODO: call this in render manager or somewhere general
	imageBasedRenderer->computePrefilter(cmd, frame);
	renderDeviceVulkan->endLabel(cmd);

    renderDeviceVulkan->beginLabel(cmd, "Forward Render Pass");
	recordDrawToTextureCommand(cmd, frame);
    renderDeviceVulkan->endLabel(cmd);

	rendererManagerVulkan->setDisplayImage(renderTarget.colorTextures[frame]);
}

void ForwardRendererVulkan::recordDrawToTextureCommand(VkCommandBuffer cmd, uint32_t imageIndex)
{
	beginRecording(cmd,renderTarget.renderPass,renderTarget.framebuffers[imageIndex]);
	{
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

		SceneManager& sceneManager = SceneManager::getInstance();
		Scene* scene = sceneManager.getActiveScene();
		if (!scene) {
			m_logger->error("No scene to render");
		}

		int index = 0;
		int lightIndex = 0;

		materialManager->bindMaterial(cmd, (void*)offscreenPipeline.get());

		for (auto& entity : scene->getEntitiesWith<TransformComponent>()) {
			TransformComponent& transform = entity.getComponent<TransformComponent>();
			const glm::mat4& entityTransform = transform.getModelMatrix();
			glm::vec3& translation = transform.translateVec;
			
			if(index >= instanceData.size()) {
				instanceData.push_back({entityTransform});
				continue;
			}

			if(entity.hasComponent<ModelComponent>()) {
				uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
				const Model* model = modelManager->getModel(modelID);

				if (!model) {
					continue;
				}
				
				auto materialManagerVulkan = (MaterialManagerVulkan*)materialManager;
				for (uint32_t meshID : model->meshIDs) {
					const Mesh* mesh = meshManager->getMesh(meshID);
					
					pushConstantLight.materialIdx = mesh->materialID;
					pushConstantLight.materialRef = materialManagerVulkan->getMaterialAddress();

					meshManager->bindMesh(meshID);

					vkCmdPushConstants(
						cmd,
						offscreenPipeline->pipelineLayout,
						VK_SHADER_STAGE_FRAGMENT_BIT,
						0,
						sizeof(PushConstantLight),
						&pushConstantLight
					);
					

					uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
					renderDeviceVulkan->draw(indexCount, numInstances, index);
				}
			}
			index++;
		}
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
	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

	void* handle = materialManager->getMaterialLayout();
	auto materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

	offscreenPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	offscreenPipeline->createGraphicsPipeline(
		"assets/shaders/spv/forwardLightPass.vert.spv",
		"assets/shaders/spv/forwardLightPass.frag.spv",
		{ descriptorSetLayout, bindlessLayout, materialLayout }, 
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

		auto createTexture = [&] (TextureVulkan*& texture,
			uint32_t w, uint32_t h,	VkFormat format,
			VkImageAspectFlagBits aspect, VkImageUsageFlags extraUsage = 0
		) {
			TextureSamplerConfig samplerConfig {};
			TextureConfig imageConfig { .width = w, .height = h, .format = format, .aspectBits = aspect};
			imageConfig.usage |= extraUsage;

			uint32_t id = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
			texture = textureManagerVulkan->getTexture(id);
		};

		createTexture(
			renderTarget.colorTextures[i],
			renderTarget.width,
			renderTarget.height,
			swapchain.swapChainImageFormat,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		);

		createTexture(
			renderTarget.depthTextures[i],
			renderTarget.width,
			renderTarget.height,
			TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device),
			VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		);

		textureManagerVulkan->registerTextureSampler(renderTarget.colorTextures[i]->id());

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
		DescriptorWriter writer{{}, descriptorSets[i] };
		descriptorManagerVulkan->writeUniform2(writer, uniformbuffersList[i]->getDescUniformBufferInfo());
		descriptorManagerVulkan->writeStorage2(writer, storagebuffersList[i]->getDescStorageBufferInfo());
		descriptorManagerVulkan->writeStorage2(writer, lightStoragebuffers[i]->getDescStorageBufferInfo());
		descriptorManagerVulkan->writeImage2(writer, shadowMapRenderer->depthMap->getDescImageInfoReadOnly());
		descriptorManagerVulkan->writeImage2(writer, shadowMapRenderer->blueNoiseImage->getDescImageInfoReadOnly());
		descriptorManagerVulkan->writeStorage2(writer, imageBasedRenderer->finalSumBuffers[i]->getDescStorageBufferInfo());
		descriptorManagerVulkan->writeImage2(writer, imageBasedRenderer->brdfLUT->getDescImageInfoReadOnly());
		descriptorManagerVulkan->writeImage2(writer, imageBasedRenderer->prefilterMap->getDescImageInfoReadOnly());
		descriptorManagerVulkan->writeImage2(writer, imageBasedRenderer->hdrImage->getDescImageInfoReadOnly());
		descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
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