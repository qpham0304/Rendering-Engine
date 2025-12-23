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
	
	_createDescriptorSets();

	return true;
}

bool RendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	renderDeviceVulkan->pipeline.destroy();

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
	uniformbuffersList[frame]->update(ubo);

	StorageBufferVulkan* ssbo = storagebuffersList[frame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	beginFrame();

	renderDeviceVulkan->render();
	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());

	endFrame();
}



void RendererVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	Timer("CPU render submission time", true);

	beginRecording(
		commandBuffer,
		renderDeviceVulkan->swapchain.renderPass,
		renderDeviceVulkan->swapchain.currentFrameBuffer()
	);

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
		if(!entity.hasComponent<ModelComponent>()) {
			continue;
		}
		
		uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
		const Model* model = modelManager->getModel(modelID);
		
		const glm::mat4& entityTransform = entity.getComponent<TransformComponent>().getModelMatrix();
		// TODO: copy the multiple all transforms to ssbo would be slow
		if (instanceData[index].model != entityTransform) {
			instanceData[index].model = entityTransform;
		}

		for (uint32_t meshID : model->meshIDs) {
			const Mesh* mesh = meshManager->getMesh(meshID);
			materialManager->bindMaterial(mesh->materialID, commandBuffer);
			meshManager->bindMesh(meshID);

			uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
			renderDeviceVulkan->draw(indexCount, numInstances, index);
		}
		index++;
	}

	renderGui(commandBuffer);

	endRecording(commandBuffer);
}

void RendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer)
{
	uint32_t imageIndex = renderDeviceVulkan->getImageIndex();
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);
	VkRenderPass vulkanRenderPass = static_cast<VkRenderPass>(renderPass);
	VkFramebuffer vulkanFrameBuffer = static_cast<VkFramebuffer>(frameBuffer);

	renderDeviceVulkan->commandPool.beginBuffer();

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

	renderDeviceVulkan->pipeline.bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport();
	renderDeviceVulkan->setScissor();
}


void RendererVulkan::endRecording(void* cmdBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	vkCmdEndRenderPass(commandBuffer);

	renderDeviceVulkan->commandPool.endBuffer();
}

void RendererVulkan::renderGui(void* commandBuffer)
{
	if (showGui) {
		guiManager->start();
		guiManager->render(commandBuffer);
		guiManager->end();
	}
}


void RendererVulkan::_createDescriptorSetLayout() {
	std::vector<VkDescriptorSetLayoutBinding> bindings = { 
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	};
	
	layoutID = descriptorManagerVulkan->createLayout(bindings);
}


void RendererVulkan::_createDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VulkanUtils::numFrames() },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VulkanUtils::numFrames() },
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, VulkanUtils::numFrames());
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

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}