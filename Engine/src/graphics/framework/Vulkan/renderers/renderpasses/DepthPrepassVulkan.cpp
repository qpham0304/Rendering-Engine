#include "DepthPrepassVulkan.h"
#include <window/AppWindow.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/scene/SceneManager.h>
#include <core/features/ServiceLocator.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/renderers/RenderDevice.h>
#include <animation/AnimationManager.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>

DepthPrepassVulkan::DepthPrepassVulkan(std::string serviceName)
	: RendererVulkan(serviceName)
{
	setFootprint("DepthPrepassVulkan", sizeof(DepthPrepassVulkan), MemorySystem::Graphics);

}

DepthPrepassVulkan::~DepthPrepassVulkan()
{

}

bool DepthPrepassVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	_createRenderTarget();
	_createDescriptor();
	_createPipeline();

	return true;
}

bool DepthPrepassVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();

	return true;
}

void DepthPrepassVulkan::onUpdate()
{

}

void DepthPrepassVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "Depth Prepass");

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageView = renderTarget.depthTexture->textureImageView;
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = { {0, 0}, {renderTarget.width, renderTarget.height} };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 0;
	renderingInfo.pDepthAttachment = &depthAttachment;

	vkCmdBeginRendering(cmd, &renderingInfo);
	_renderDepth(cmd, currentFrame);
	vkCmdEndRendering(cmd);

	renderDeviceVulkan->endLabel(cmd);
}

void DepthPrepassVulkan::addToGraph(RenderGraphVulkan& graph)
{
	// same texture as last frame unless the render size changed, then the graph recreated it
	_createRenderTarget();
	needResize = false;		// nothing else in this pass depends on the window size

	RenderPassNode node{};
	node.name = "DepthPrepass";
	node.writes = { { renderTarget.depthHandle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL } };

	node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
		Camera* camera = SceneManager::cameraController;

		if(!camera) {
			return;
		}

		render(*camera);
	};

	graph.addPass(node);
}

void DepthPrepassVulkan::_renderDepth(VkCommandBuffer cmd, uint32_t currentFrame)
{
	depthPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport(renderTarget.width, renderTarget.height);
	renderDeviceVulkan->setScissor(renderTarget.width, renderTarget.height);

	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		depthPipeline->pipelineLayout,
		0,
		1,
		&descriptorSets[currentFrame],
		0,
		nullptr
	);

	auto animationManager = &ServiceLocator::GetService<AnimationManager>("AnimationManager");
	pushConstant.objectsRef = rendererManagerVulkan->getObjectsAddress();
	pushConstant.boneMatricesRef = bufferManagerVulkan->getVirtualMemoryAddress(animationManager->getAnimationBufferAddressID());

	vkCmdPushConstants(
		cmd,
		depthPipeline->pipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(Common::PushBDA),
		&pushConstant
	);

	rendererManagerVulkan->drawObjects(cmd);
}


#pragma region setup
void DepthPrepassVulkan::_createRenderTarget()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	renderTarget.width = swapchain.swapChainExtent.width;
	renderTarget.height = swapchain.swapChainExtent.height;
	renderTarget.depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

	RGTextureDesc desc{};
	desc.config.format = renderTarget.depthFormat;
	desc.config.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	desc.config.aspectBits = VK_IMAGE_ASPECT_DEPTH_BIT;
	desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
	renderTarget.depthHandle = graph.createRenderTexture("Scene.Depth", desc);
	renderTarget.depthTexture = graph.getTexture(renderTarget.depthHandle);
}

void DepthPrepassVulkan::_createDescriptor()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	};
	layoutID = descriptorManagerVulkan->createLayout(bindings);

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);
	_updateDescriptor();
}

void DepthPrepassVulkan::_updateDescriptor()
{
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);
	auto uniformbuffersList = rendererManagerVulkan->getBufferUBO();

	for (size_t i = 0; i < VulkanUtils::numFrames(); i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}

void DepthPrepassVulkan::_createPipeline()
{
	PipelineConfigInfo depthConfig = VulkanPipeline::defaultPipelineConfigInfo(0);
	depthConfig.renderPass = VK_NULL_HANDLE;

	AttachmentsInfo attachmentsInfo{};
	attachmentsInfo.depthAttachmentFormat = renderTarget.depthFormat;
	attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);

	depthPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	depthPipeline->createGraphicsPipelineDynamic(
		"assets/shaders/spv/depthPrepass.vert.spv",
		"assets/shaders/spv/depthPrepass.frag.spv",
		depthConfig,
		attachmentsInfo,
		vertexInputInfo,
		{ descriptorSetLayout },
		sizeof(Common::PushBDA)
	);
}

void DepthPrepassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();

	_createRenderTarget();
	_createPipeline();
}

void DepthPrepassVulkan::_cleanupResources()
{
	depthPipeline->destroy();
}
#pragma endregion setup
