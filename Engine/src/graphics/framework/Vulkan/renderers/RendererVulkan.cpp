#include "RendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "graphics/RenderDevice.h"
#include "graphics/framework/Vulkan/RenderDeviceVulkan.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <gui/GuiManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>

#include "imgui.h" // TODO: remove it once done

RendererVulkan::RendererVulkan() 
	: Renderer("RendererVulkan")
{

}

RendererVulkan::~RendererVulkan()
{

}

int RendererVulkan::init(WindowConfig config)
{
	Service::init(config);

	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	textureManager = &ServiceLocator::GetService<TextureManager>("TextureManager");
	meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
	modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
	guiManager = &ServiceLocator::GetService<GuiManager>("ImGuiManager");
	
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);
	if (!renderDeviceVulkan) {
		throw std::runtime_error("Failed to accquire render device");
		return -1;
	}

	//TODO: this should be set via service locator
	bufferManager = &renderDeviceVulkan->vulkanBufferManager;

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

	renderDeviceVulkan->setPushConstantRange(sizeof(PushConstantData));


	return 0;
}

int RendererVulkan::onClose()
{
	return 0;
}

void RendererVulkan::onUpdate()
{

}


void RendererVulkan::beginFrame()
{
	renderDeviceVulkan->beginFrame();
}

void RendererVulkan::endFrame()
{
	renderDeviceVulkan->endFrame();
}
void RendererVulkan::render(Camera& camera, Scene* scene)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	VulkanDevice::UniformBufferObject ubo{};


	ubo.model = glm::mat4(1.0);
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.6f, 1.0f, 0.2f));
	ubo.model = glm::scale(ubo.model, glm::vec3(0.5f, 0.5f, 0.5f));
	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.proj[1][1] *= -1;
	renderDeviceVulkan->vulkanBufferManager.updateUniformBuffer(renderDeviceVulkan->getCurrentFrameIndex(), ubo);

	beginFrame();

	renderDeviceVulkan->render();
	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
		recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());

	endFrame();
}

void RendererVulkan::shutdown()
{

}

void RendererVulkan::addMesh()
{

}

void RendererVulkan::addModel(std::string_view path)
{
	modelID = modelManager->loadModel(path);

	//TODO: this unbound this to use the model's texture
	uint32_t id = textureManager->loadTexture("assets/textures/mobi-padoru.png");
	renderDeviceVulkan->texture = dynamic_cast<TextureVulkan*>(textureManager->getTexture(id));

	renderDeviceVulkan->createDescriptorSets();
	renderDeviceVulkan->createTextureViewDescriptorSet();
}

void RendererVulkan::beginRecording(void* cmdBuffer)
{
	uint32_t imageIndex = renderDeviceVulkan->getImageIndex();
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	renderDeviceVulkan->commandPool.beginBuffer();
	assert(imageIndex < renderDeviceVulkan->swapchain.swapChainFramebuffers.size() && "imageIndex out of range of framebuffers");

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderDeviceVulkan->swapchain.renderPass;
	renderPassInfo.framebuffer = renderDeviceVulkan->swapchain.swapChainFramebuffers[imageIndex];
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

void RendererVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	Timer("Draw recoding time", true);

	beginRecording(commandBuffer);


	// issue draw model
	Model* aruModel = modelManager->getModel(modelID);
	for (uint32_t id : aruModel->meshIDs) {
		uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
		VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
		
		vkCmdBindDescriptorSets(
			cmdBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			renderDeviceVulkan->pipeline.pipelineLayout,
			0,
			1,
			&renderDeviceVulkan->descriptorSets[currentFrame],
			0,
			nullptr
		);

		vkCmdPushConstants(
			cmdBuffer,
			renderDeviceVulkan->pipeline.pipelineLayout,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0, // offset
			sizeof(PushConstantData),
			&pushConstantData
		);

		const MeshManager::MeshData* meshData = meshManager->getMeshData(id);
		renderDeviceVulkan->vulkanBufferManager.bind(meshData->vertexBufferID);
		renderDeviceVulkan->vulkanBufferManager.bind(meshData->indexBufferID);

		const Mesh* mesh = meshManager->getMesh(id);
		uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
		renderDeviceVulkan->draw(indexCount, 1);
	}

	renderGui(commandBuffer);

	endRecording(commandBuffer);
}

void RendererVulkan::renderGui(void* commandBuffer)
{
	if (showGui) {
		guiManager->start();
		ImGui::Begin("Texture View");
		ImGui::BeginChild("Image View");
		ImGui::Image((ImTextureID)renderDeviceVulkan->imguiTextureDescriptorSet, ImVec2(500, 500));
		ImGui::EndChild();
		ImGui::End();
		guiManager->render(commandBuffer);
		guiManager->end();
	}
}
