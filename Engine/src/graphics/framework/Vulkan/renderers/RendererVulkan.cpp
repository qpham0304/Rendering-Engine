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

#include "imgui.h" // TODO: remove it once done

RendererVulkan::RendererVulkan() 
	:	Renderer("RendererVulkan")
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

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManager");
	bufferManagerVulkan = &static_cast<BufferManagerVulkan&>(bufferManager);
	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = &static_cast<DescriptorManagerVulkan&>(descriptorManager);
	
	textureManager = &ServiceLocator::GetService<TextureManager>("TextureManager");
	meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
	modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
	guiManager = &ServiceLocator::GetService<GuiManager>("ImGuiManager");
	
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);
	if (!renderDeviceVulkan) {
		throw std::runtime_error("Failed to accquire render device");
		return -1;
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
	_createDescriptorPool();

	renderDeviceVulkan->pipeline.create();
	renderDeviceVulkan->pipeline.createGraphicsPipeline(descriptorSetLayout, renderDeviceVulkan->swapchain.renderPass, sizeof(PushConstantData));

	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));

	return 0;
}

int RendererVulkan::onClose()
{
	bufferManagerVulkan->onClose();

	renderDeviceVulkan->pipeline.destroy();

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

	UniformBufferObject ubo{};
	ubo.model = glm::mat4(1.0);
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.6f, 1.0f, 0.2f));
	ubo.model = glm::scale(ubo.model, glm::vec3(0.5f, 0.5f, 0.5f));
	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.proj[1][1] *= -1;
	uniformbuffersList[renderDeviceVulkan->getCurrentFrameIndex()]->updateUniformBuffer(ubo);


	beginFrame();

	renderDeviceVulkan->render();
	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());

	endFrame();
}

void RendererVulkan::addMesh()
{

}

void RendererVulkan::addModel(std::string_view path)
{
	modelID = modelManager->loadModel(path);

	//TODO: this unbound this to use the model's texture
	uint32_t id = textureManager->loadTexture("assets/textures/mobi-padoru.png");
	
	texture = dynamic_cast<TextureVulkan*>(textureManager->getTexture(id));


	_createDescriptorSets();
	_createTextureViewDescriptorSet();
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
			&descriptorSets[currentFrame],
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

		meshManager->bindMesh(id);

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
		ImGui::Image((ImTextureID)imguiTextureDescriptorSet, ImVec2(500, 500));
		ImGui::EndChild();
		ImGui::End();
		guiManager->render(commandBuffer);
		guiManager->end();
	}
}


void RendererVulkan::_createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.pImmutableSamplers = nullptr;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> bindings = { uboLayoutBinding, samplerLayoutBinding };
	layoutID = descriptorManagerVulkan->createLayout(bindings);
	descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
}


void RendererVulkan::_createDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VulkanUtils::numFrames()},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VulkanUtils::numFrames()},
	};
	poolID = descriptorManagerVulkan->createPool(poolSizes, VulkanUtils::numFrames());
	descriptorPool = descriptorManagerVulkan->getDescriptorPool(poolID);
}

void RendererVulkan::_createDescriptorSets()
{
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, VulkanUtils::numFrames());
	descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = texture->textureImageView;
		imageInfo.sampler = texture->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], bufferInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}


void RendererVulkan::_createTextureViewDescriptorSet()
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> bindings = { samplerLayoutBinding };
	uint32_t imGuilayoutID = descriptorManagerVulkan->createLayout(bindings);
	imguiDescriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(imGuilayoutID);

	std::vector<VkDescriptorPoolSize> poolSizes = { 
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} 
	};
	uint32_t imGuipoolID = descriptorManagerVulkan->createPool(poolSizes, VulkanUtils::numFrames());
	imguiDescriptorPool = descriptorManagerVulkan->getDescriptorPool(imGuipoolID);

	uint32_t imguiSetID = descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1);
	imguiTextureDescriptorSet = descriptorManagerVulkan->getDescriptorSet(imguiSetID)[0];

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture->textureImageView;
	imageInfo.sampler = texture->textureSampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = imguiTextureDescriptorSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(renderDeviceVulkan->device, 1, &write, 0, nullptr);
}

