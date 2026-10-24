#include "GeometryPassVulkan.h"
#include "DepthPrepassVulkan.h"
#include <window/AppWindow.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/scene/SceneManager.h>
#include <core/features/ServiceLocator.h>
#include <core/events/EventManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/renderers/RenderDevice.h>
#include <animation/AnimationManager.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <array>

GeometryPassVulkan::GeometryPassVulkan(std::string serviceName)
	: RendererVulkan(serviceName)
{
	setFootprint("GeometryPassVulkan", sizeof(GeometryPassVulkan), MemorySystem::Graphics);

}

GeometryPassVulkan::~GeometryPassVulkan()
{

}

bool GeometryPassVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	_createRenderTargets();
	_createDescriptor();
	_createPipelines();

	return true;
}

bool GeometryPassVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();

	return true;
}

void GeometryPassVulkan::onUpdate()
{

}

void GeometryPassVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "Geometry Pass");

	auto colorAttachment = [](TextureVulkan* texture, VkClearColorValue clear) {
		VkRenderingAttachmentInfo attachment{};
		attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachment.imageView = texture->textureImageView;
		attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.clearValue.color = clear;
		return attachment;
	};

	// order matches the gBuffer.frag output locations
	std::array<VkRenderingAttachmentInfo, 6> colorAttachments = {
		colorAttachment(renderTarget.gBufferPos, { 0.0f, 0.0f, 0.0f, 1.0f }),
		colorAttachment(renderTarget.gBufferNorm, { 0.0f, 0.0f, 0.0f, 1.0f }),
		colorAttachment(renderTarget.gBufferAlbedo, { 0.0f, 0.0f, 0.0f, 1.0f }),
		colorAttachment(renderTarget.gPBR, { 1.0f, 1.0f, 0.0f, 1.0f }),
		colorAttachment(renderTarget.gBufferEmissive, { 1.0f, 1.0f, 0.0f, 1.0f }),
		colorAttachment(renderTarget.gBufferMotion, { 1.0f, 1.0f, 0.0f, 1.0f }),
	};

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageView = renderTarget.depthTexture->textureImageView;
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;	// written by the depth prepass, only tested here
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = { {0, 0}, {renderTarget.width, renderTarget.height} };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
	renderingInfo.pColorAttachments = colorAttachments.data();
	renderingInfo.pDepthAttachment = &depthAttachment;

	vkCmdBeginRendering(cmd, &renderingInfo);
	_renderGeometryPass(cmd, currentFrame);
	vkCmdEndRendering(cmd);

	renderDeviceVulkan->endLabel(cmd);
}

void GeometryPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
	// same textures as last frame unless the render size changed, then the graph recreated them
	_createRenderTargets();
	needResize = false;		// nothing else in this pass depends on the window size

	RenderPassNode node{};
    node.name = "gBuffer";
    node.reads  = { { renderTarget.depthHandle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL } };	// depth test only, writes are off
    node.writes = {
		{ renderTarget.posHandle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ renderTarget.normHandle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ renderTarget.albedoHandle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ renderTarget.pbrHandle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ renderTarget.emissiveHandle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ renderTarget.motionHandle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
	};

    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        Scene* scene = SceneManager::getInstance().getActiveScene();
        Camera* camera = SceneManager::cameraController;

        if(!camera) {
            return;
        }

        render(*camera);
    };

    graph.addPass(node);
}

void GeometryPassVulkan::_renderGeometryPass(VkCommandBuffer cmd, uint32_t currentFrame)
{
	gPassPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport(renderTarget.width, renderTarget.height);
	renderDeviceVulkan->setScissor(renderTarget.width, renderTarget.height);

	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		gPassPipeline->pipelineLayout,
		0,
		1,
		&descriptorSets[currentFrame],
		0,
		nullptr
	);

	materialManager->bindMaterial(cmd, (void*)gPassPipeline.get());

	auto animationManager = &ServiceLocator::GetService<AnimationManager>("AnimationManager");
	pushConstant.objectsRef = rendererManagerVulkan->getObjectsAddress();
	pushConstant.boneMatricesRef = bufferManagerVulkan->getVirtualMemoryAddress(animationManager->getAnimationBufferAddressID());

	vkCmdPushConstants(
		cmd,
		gPassPipeline->pipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(Common::PushBDA),
		&pushConstant
	);

	rendererManagerVulkan->drawObjects(cmd);
}


#pragma region setup
void GeometryPassVulkan::_createRenderTargets()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	renderTarget.width = swapchain.swapChainExtent.width;
	renderTarget.height = swapchain.swapChainExtent.height;
	renderTarget.depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

	RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
	auto createTexture = [&](const char* name, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlagBits aspect, RGHandle& handle) {
		RGTextureDesc desc{};
		desc.config.format = format;
		desc.config.usage = usage;
		desc.config.aspectBits = aspect;
		desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		handle = graph.createRenderTexture(name, desc);
		return graph.getTexture(handle);
	};

	const VkImageUsageFlags colorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	renderTarget.gBufferPos = createTexture("GBuffer.Position", VK_FORMAT_R16G16B16A16_SFLOAT,
		colorUsage | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, renderTarget.posHandle);
	renderTarget.gBufferNorm = createTexture("GBuffer.Normal", VK_FORMAT_R16G16B16A16_SFLOAT,
		colorUsage | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, renderTarget.normHandle);
	renderTarget.gBufferAlbedo = createTexture("GBuffer.Albedo", VK_FORMAT_R8G8B8A8_UNORM,
		colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, renderTarget.albedoHandle);
	renderTarget.gPBR = createTexture("GBuffer.PBR", VK_FORMAT_R8G8B8A8_UNORM,
		colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, renderTarget.pbrHandle);
	renderTarget.gBufferEmissive = createTexture("GBuffer.Emissive", VK_FORMAT_R16G16B16A16_SFLOAT,
		colorUsage | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, renderTarget.emissiveHandle);
	renderTarget.gBufferMotion = createTexture("GBuffer.Motion", VK_FORMAT_R16G16_SFLOAT,
		colorUsage | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, renderTarget.motionHandle);

	auto depthPrepass = dynamic_cast<DepthPrepassVulkan*>(rendererManagerVulkan->getRenderer("DepthPrepassVulkan"));
	renderTarget.depthTexture = depthPrepass->renderTarget.depthTexture;
	renderTarget.depthHandle = depthPrepass->renderTarget.depthHandle;
}

void GeometryPassVulkan::_createDescriptor()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	};
	layoutID = descriptorManagerVulkan->createLayout(bindings);

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);

	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, VulkanUtils::numFrames());
	_updateDescriptor();
}

void GeometryPassVulkan::_updateDescriptor()
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

void GeometryPassVulkan::_createPipelines()
{
	PipelineConfigInfo gBufferConfig = VulkanPipeline::defaultPipelineConfigInfo(6);
	gBufferConfig.renderPass = VK_NULL_HANDLE;
	// depth is final after the prepass, only the visible surface passes (gl_Position is invariant in both shaders)
	gBufferConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
	gBufferConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	AttachmentsInfo attachmentsInfo{};
	attachmentsInfo.colorAttachmentFormats = {
		renderTarget.gBufferPos->format,
		renderTarget.gBufferNorm->format,
		renderTarget.gBufferAlbedo->format,
		renderTarget.gPBR->format,
		renderTarget.gBufferEmissive->format,
		renderTarget.gBufferMotion->format,
	};
	attachmentsInfo.depthAttachmentFormat = renderTarget.depthFormat;
	attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);

	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

	void* handle = materialManager->getMaterialLayout();
	auto materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

	gPassPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	gPassPipeline->createGraphicsPipelineDynamic(
		"assets/shaders/spv/gBuffer.vert.spv",
		"assets/shaders/spv/gBuffer.frag.spv",
		gBufferConfig,
		attachmentsInfo,
		vertexInputInfo,
		{ descriptorSetLayout, bindlessLayout, materialLayout },
		sizeof(Common::PushBDA)
	);
}

void GeometryPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();

	_createRenderTargets();
	_createPipelines();
}

void GeometryPassVulkan::_cleanupResources()
{
	gPassPipeline->destroy();
}
#pragma endregion setup
