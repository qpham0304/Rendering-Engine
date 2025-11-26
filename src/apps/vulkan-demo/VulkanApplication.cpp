#include "VulkanApplication.h"
#include <cstring>
#include <cassert>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"
#include "../../src/window/AppWindow.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/graphics/framework/Vulkan/RenderDeviceVulkan.h"	//TODO: remove conrete type access dependency
#include "src/graphics/framework/OpenGL/core/ModelOpenGL.h"

ModelOpenGL* modelPtr = nullptr;

const std::vector<VulkanDevice::Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

void VulkanApplication::pushLayer(Layer* layer)
{
	layerManager->addLayer(layer);
}

void VulkanApplication::init(WindowConfig config)
{
	windowConfig = config;

	ServiceLocator::setContext(&serviceLocator);
	engineLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	renderDevice = platformFactory.createRenderDevice(windowConfig.renderPlatform);
	renderDeviceVulkan = static_cast<RenderDeviceVulkan*>(renderDevice.get());
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);

	ServiceLocator::supportingServices();
}

void VulkanApplication::start()
{
	appWindow->init(windowConfig);

	camera.init(AppWindow::getWidth(), AppWindow::getHeight(),
		glm::vec3(2.0f, 2.0f, 2.0f),
		glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(2.0f, 2.0f, 2.0f))
	);


	EventManager::getInstance().subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		camera.updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
	});

	EventManager& eventManager = EventManager::getInstance();
	eventManager.subscribe(EventType::MouseScrolled, [this](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		camera.scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
	});

	eventManager.subscribe(EventType::MouseMoved, [this](Event& event) {
	MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
		//camera.processMouse();
		//camera.processKeyboard();
		camera.processInput();
		//if (showGui) {
		//	mouseEvent.Handled = true;	// block mouse event from other layers
		//}
	});


	EventManager::getInstance().subscribe(EventType::WindowClose, [this](Event& event) {
		isRunning = false;
	});

	EventManager::getInstance().subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_1) {
			pushConstantData.flag = !pushConstantData.flag;
		}
		else if (keyPressedEvent.keyCode == KEY_2) {
			showGui = !showGui;
		}
	});

	pushConstantData.color = glm::vec3(1.0f, 1.0f, 0.0f);
	pushConstantData.range = glm::vec3(1.0f, 1.0f, 1.0f);
	pushConstantData.data = 0.1f;


	renderDeviceVulkan->setPushConstantRange(sizeof(PushConstantData));
	renderDeviceVulkan->init(windowConfig);

	renderDeviceVulkan->vulkanBuffer.createVertexBuffer(vertices, renderDeviceVulkan->commandPool.commandPool);
	renderDeviceVulkan->vulkanBuffer.createIndexBuffer(indices, renderDeviceVulkan->commandPool.commandPool);
	renderDeviceVulkan->vulkanBuffer.createCombinedBuffer(vertices, indices, renderDeviceVulkan->commandPool.commandPool);

	guiManager->init(windowConfig);

	layerManager = std::make_unique<LayerManager>(serviceLocator);

	editorLayer = new EditorLayer();
	//editorLayer->init(guiManager.get());

	//Model model("Models/aru/aru.gltf");
	//modelPtr = new Model("Models/aru/aru.gltf");

	//auto meshes = modelPtr->meshes;
}


void VulkanApplication::run() {
	//pushLayer(editorLayer);

	while (isRunning) {
		appWindow->onUpdate();
		float dt = appWindow->getTime();
		eventManager.onUpdate();
		sceneManager.onUpdate(dt);
		layerManager->onUpdate();

		camera.onUpdate();
		camera.processInput();

		render();

		if (showGui) {
			//guiManager->start();
			sceneManager.onGuiUpdate(dt);
			layerManager->onGuiUpdate();
			guiManager->end();
		}
	}
}

void VulkanApplication::end()
{
	renderDeviceVulkan->waitIdle();
	guiManager->onClose();
	renderDeviceVulkan->onClose();
	appWindow->onClose();
}

void VulkanApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	renderDeviceVulkan->commandPool.beginBuffer();
	assert(imageIndex < renderDeviceVulkan->swapchain.swapChainFramebuffers.size() && "imageIndex out of range of framebuffers");

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderDeviceVulkan->swapchain.renderPass;
	renderPassInfo.framebuffer = renderDeviceVulkan->swapchain.swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = renderDeviceVulkan->swapchain.swapChainExtent;

	
	//VkClearValue clearColor = 
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
	
	renderDeviceVulkan->shaderSetUniform();
	vkCmdPushConstants(
		renderDeviceVulkan->commandPool.commandBuffers[renderDeviceVulkan->getCurrentFrame()],
		renderDeviceVulkan->pipeline.pipelineLayout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0, // offset
		sizeof(pushConstantData),
		&pushConstantData
	);

	// issue draw
	renderDeviceVulkan->vulkanBuffer.bind();
	renderDeviceVulkan->draw(static_cast<uint32_t>(indices.size()), 1);


	if (showGui) {
		guiManager->start();
		ImGui::BeginChild("Image View");
		ImGui::Image((ImTextureID)renderDeviceVulkan->imguiTextureDescriptorSet, ImVec2(500, 500));
		ImGui::EndChild();
		guiManager->render(commandBuffer);

	}

	vkCmdEndRenderPass(commandBuffer);

	renderDeviceVulkan->commandPool.endBuffer();
}

void VulkanApplication::render()
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	VulkanDevice::UniformBufferObject ubo{};
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.proj[1][1] *= -1;

	renderDeviceVulkan->beginFrame();

	renderDeviceVulkan->vulkanBuffer.updateUniformBuffer(renderDeviceVulkan->getCurrentFrame(), ubo);

	renderDeviceVulkan->render();
	recordCommandBuffer(
		renderDeviceVulkan->commandPool.commandBuffers[renderDeviceVulkan->getCurrentFrame()],
		renderDeviceVulkan->getImageIndex()
	);

	renderDeviceVulkan->endFrame();
}