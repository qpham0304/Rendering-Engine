#include "GeometryPassVulkan.h"
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

}

GeometryPassVulkan::~GeometryPassVulkan()
{

}

bool GeometryPassVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	_createRenderTargets();

	size_t bufferSize = Common::MAX_INSTANCES * sizeof(StorageBufferObject);
	instanceData.resize(Common::MAX_INSTANCES);
	instanceDataPrev.resize(Common::MAX_INSTANCES);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);
	bufferManagerVulkan->createStorageBuffers(prevStoragebufferList, bufferSize);

	objects.resize(Common::MAX_INSTANCES);
	size_t objectsBufferSize = Common::MAX_INSTANCES * sizeof(Common::ObjectRefs);
	for(uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
		uint32_t bufferID = bufferManagerVulkan->createBufferDeviceAddress(objectsBufferSize);
		auto deviceAddress = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(bufferID));
		objDeviceAddressBufferIDs.push_back(bufferID);
		objDeviceAddresses.push_back(deviceAddress->getAddress());
	}

	_createDescriptor();
	_createPipelines();

	auto materialManagerVulkan = static_cast<MaterialManagerVulkan*>(materialManager);
	materialsAddress = materialManagerVulkan->getMaterialAddress();

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
	_updateSceneData(currentFrame);

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
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

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
	if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    auto gBufferPos = graph.importTexture(renderTarget.gBufferPos, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto gBufferNorm = graph.importTexture(renderTarget.gBufferNorm, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto gBufferAlbedo = graph.importTexture(renderTarget.gBufferAlbedo, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto gPBR = graph.importTexture(renderTarget.gPBR, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto gBufferEmissive = graph.importTexture(renderTarget.gBufferEmissive, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto gBufferMotion = graph.importTexture(renderTarget.gBufferMotion, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto depthTexture = graph.importTexture(renderTarget.depthTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
	RenderPassNode node{};
    node.name = "gBuffer";
    node.reads  = { };	//TODO: add a depth prepass before everything
    node.writes = {
		{ gBufferPos, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ gBufferNorm, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ gBufferAlbedo, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ gPBR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ gBufferEmissive, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ gBufferMotion, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ depthTexture, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
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

void GeometryPassVulkan::_updateSceneData(uint32_t currentFrame)
{
	instanceDataPrev = std::move(instanceData);
	instanceData.clear();

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
		return;
	}
	auto entities = scene->getEntitiesWith<TransformComponent>();
	int currentDrawIdx = 0;

	for (auto& entity : entities) {
		auto& transform = entity.getComponent<TransformComponent>();

		if(entity.hasComponent<ModelComponent>()) {
			uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
			const Model* model = modelManager->getModel(modelID);

			uint32_t meshIdx = 0;
			for (uint32_t meshID : model->meshIDs) {
				glm::mat4 worldMatrix = transform.getModelMatrix();

				if(entity.hasComponent<LightProbeComponent>()) {	//TODO: this couple the probe data to the renderer so it need to be handled better
					auto& lightProbeComponent = entity.getComponent<LightProbeComponent>();
					worldMatrix = glm::translate(worldMatrix, glm::vec3(lightProbeComponent.probeGrid[meshIdx]));
				}
				instanceData.push_back({ worldMatrix });

				Common::ObjectRefs desc{};
				desc.materialsRef = materialsAddress;
				const MeshManager::MeshData& meshData = meshManager->getMeshData(meshID);
				auto* bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.matIndicesBDA_ID));
				desc.materialIndicesRef = bdaBuffer->getAddress();
				bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.vertexBDA_ID));
				desc.vertexAddress = bdaBuffer->getAddress();
				bdaBuffer = static_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(meshData.indexBDA_ID));
				desc.indexAddress = bdaBuffer->getAddress();

				objects[currentDrawIdx] = desc;
				currentDrawIdx++;
				meshIdx++;
			}
		}
		else {
			instanceData.push_back({ transform.getModelMatrix() });
			objects[currentDrawIdx] = {};
			currentDrawIdx++;
		}
	}

	StorageBufferVulkan* ssbo = storagebuffersList[currentFrame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	StorageBufferVulkan* ssboPrev = prevStoragebufferList[currentFrame];
	ssboPrev->update(instanceDataPrev.data(), instanceDataPrev.size() * sizeof(StorageBufferObject));

	size_t buffersize = Common::MAX_INSTANCES * sizeof(Common::ObjectRefs);
	bufferManagerVulkan->updateBufferDeviceAddress(objDeviceAddressBufferIDs[currentFrame], objects.data(), buffersize);
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

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if (!scene) {
		m_logger->error("No scene to render");
		return;
	}

	int objectsIndex = 0;

	materialManager->bindMaterial(cmd, (void*)gPassPipeline.get());
	auto animationManager = &ServiceLocator::GetService<AnimationManager>("AnimationManager");

	// same traversal as _updateSceneData so objectsIndex lines up with the objects table
	for (auto& entity : scene->getEntitiesWith<TransformComponent>()) {
		if(entity.hasComponent<ModelComponent>()) {
			uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
			const Model* model = modelManager->getModel(modelID);

			uint32_t boneMatricesIdx = 0;
			if(entity.hasComponent<SkinningComponent>()) {
				boneMatricesIdx = entity.getComponent<SkinningComponent>().boneMatricesID;
			}

			for (uint32_t meshID : model->meshIDs) {
				uint32_t bufferID = animationManager->getAnimationBufferAddressID();
				pushConstant.objectsRef = objDeviceAddresses[currentFrame];
				pushConstant.objectIdx  = objectsIndex;
				pushConstant.boneMatricesRef = bufferManagerVulkan->getVirtualMemoryAddress(bufferID);
				pushConstant.boneMatricesIdx = boneMatricesIdx;

				vkCmdPushConstants(
					cmd,
					gPassPipeline->pipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(Common::PushBDA),
					&pushConstant
				);

				Mesh* mesh = const_cast<Mesh*>(meshManager->getMesh(meshID));
				meshManager->bindMesh(meshID);
				renderDeviceVulkan->draw(mesh->indices.size(), numInstances, objectsIndex);

				objectsIndex++;
			}
		} else {
			objectsIndex++;
		}
	}
}


#pragma region setup
void GeometryPassVulkan::_createRenderTargets()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	renderTarget.width = swapchain.swapChainExtent.width;
	renderTarget.height = swapchain.swapChainExtent.height;
	renderTarget.depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

	auto createTexture = [&] (VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect) -> TextureVulkan* {
		uint32_t id = textureManagerVulkan->createTexture();
		auto* texture = static_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
		texture->format = format;	// barriers and pipeline formats read it back

		TextureManagerVulkan::createImage(
			renderTarget.width,
			renderTarget.height,
			format,
			VK_IMAGE_TILING_OPTIMAL,
			usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			texture->textureImage,
			texture->textureImageMemory,
			1,
			renderDeviceVulkan->device
		);

		TextureManagerVulkan::createImageView(
			texture->textureImage,
			texture->textureImageView,
			format,
			aspect,
			1,
			renderDeviceVulkan->device
		);

		TextureManagerVulkan::createTextureSampler(
			texture->textureSampler,
			renderDeviceVulkan->device,
			TextureManagerVulkan::createLinearSampler(renderDeviceVulkan->device)
		);

		VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
		TextureManagerVulkan::transitionImageLayout(
			cmd,
			texture->textureImage,
			format,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			1,
			1,
			renderDeviceVulkan
		);
		renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

		return texture;
	};

	renderTarget.gBufferPos = createTexture(
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	renderTarget.gBufferNorm = createTexture(
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	renderTarget.gBufferAlbedo = createTexture(
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	renderTarget.gPBR = createTexture(
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	renderTarget.gBufferEmissive = createTexture(
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	renderTarget.gBufferMotion = createTexture(
		VK_FORMAT_R16G16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT
	);

	renderTarget.depthTexture = createTexture(
		renderTarget.depthFormat,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT
	);

	rendererManagerVulkan->addRenderTexture("gBufferPos", { renderTarget.gBufferPos->id() });
	rendererManagerVulkan->addRenderTexture("gBufferNorm", { renderTarget.gBufferNorm->id() });
	rendererManagerVulkan->addRenderTexture("gBufferAlbedo", { renderTarget.gBufferAlbedo->id() });
	rendererManagerVulkan->addRenderTexture("gPBR", { renderTarget.gPBR->id() });
	rendererManagerVulkan->addRenderTexture("gBufferEmissive", { renderTarget.gBufferEmissive->id() });
	rendererManagerVulkan->addRenderTexture("gBufferMotion", { renderTarget.gBufferMotion->id() });
	rendererManagerVulkan->addRenderTexture("deferredDepth", { renderTarget.depthTexture->id() });
}

void GeometryPassVulkan::_createDescriptor()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	};
	layoutID = descriptorManagerVulkan->createLayout(bindings);

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 2 },
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

		VkDescriptorBufferInfo ssboInfo{};
		ssboInfo.buffer = static_cast<VkBuffer>(*storagebuffersList[i]);
		ssboInfo.offset = 0;
		ssboInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo ssboInfoPrev{};
		ssboInfoPrev.buffer = static_cast<VkBuffer>(*prevStoragebufferList[i]);
		ssboInfoPrev.offset = 0;
		ssboInfoPrev.range = VK_WHOLE_SIZE;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 2, ssboInfoPrev);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}

void GeometryPassVulkan::_createPipelines()
{
	PipelineConfigInfo gBufferConfig = VulkanPipeline::defaultPipelineConfigInfo(6);
	gBufferConfig.renderPass = VK_NULL_HANDLE;

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

	auto bindingDescription = VulkanDevice::VertexVulkan::getBindingDescription();
	auto attributeDescriptions = VulkanDevice::VertexVulkan::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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
	textureManagerVulkan->destroy(renderTarget.gBufferPos->id());
	textureManagerVulkan->destroy(renderTarget.gBufferNorm->id());
	textureManagerVulkan->destroy(renderTarget.gBufferAlbedo->id());
	textureManagerVulkan->destroy(renderTarget.gPBR->id());
	textureManagerVulkan->destroy(renderTarget.gBufferEmissive->id());
	textureManagerVulkan->destroy(renderTarget.gBufferMotion->id());
	textureManagerVulkan->destroy(renderTarget.depthTexture->id());
	gPassPipeline->destroy();
}
#pragma endregion setup
