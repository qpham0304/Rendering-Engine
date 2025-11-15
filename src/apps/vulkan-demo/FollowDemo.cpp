#include "FollowDemo.h"
#include <cstring>
#include <cassert>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"
#include "../../src/window/AppWindow.h"
#include "../../src/core/events/EventManager.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_vulkan.h>


const std::vector<VulkanDevice::Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

void Demo::run() {
	WindowConfig windowConfig{};
	windowConfig.title = "Vulkan Demo Application";
	windowConfig.windowPlatform = WindowPlatform::GLFW;
	windowConfig.renderPlatform = RenderPlatform::VULKAN;
	windowConfig.guiPlatform = GuiPlatform::IMGUI;
	windowConfig.width = 1920;
	windowConfig.height = 1080;
	windowConfig.vsync = false;

	ServiceLocator::setContext(&serviceLocator);
	engineLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);
	renderDevice = platformFactory.createRenderDevice(windowConfig.renderPlatform);
	renderDeviceVulkan = static_cast<RenderDeviceVulkan*>(renderDevice.get());


	appWindow->init(windowConfig);
	windowHandle = static_cast<GLFWwindow*>(AppWindow::getWindowHandle());


	camera.init(AppWindow::getWidth(), AppWindow::getHeight(),
		glm::vec3(2.0f, 2.0f, 2.0f),
		glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(2.0f, 2.0f, 2.0f)));

	//orbitCamera.init(AppWindow::getWidth(), AppWindow::getHeight(),
	//	glm::vec3(2.0f, 2.0f, 2.0f),
	//	glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(2.0f, 2.0f, 2.0f)));


	EventManager::getInstance().subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		camera.updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
	});

	EventManager& eventManager = EventManager::getInstance();
	eventManager.subscribe(EventType::MouseScrolled, [this](Event& event) {
	MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		camera.scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
	});

	//eventManager.subscribe(EventType::MouseMoved, [this](Event& event) {
	//MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
	//	camera.processMouse();
	//});

	EventManager::getInstance().subscribe(EventType::WindowClose, [this](Event& event) {
		isRunning = false;
	});

	pushConstantData.color = glm::vec3(1.0f, 1.0f, 0.0f);
	pushConstantData.range = glm::vec3(1.0f, 1.0f, 1.0f);
	pushConstantData.data = 0.1f;

	initVulkan();


	// descriptorpool creation for imgui
	createGuiDescriptorPool();
	guiManager->init(windowConfig);
	initGuiContext();


	mainLoop();
	cleanup();
}


void Demo::initVulkan() {
	renderDeviceVulkan->init(windowConfig);

	renderDeviceVulkan->vulkanBuffer.createVertexBuffer(vertices, renderDeviceVulkan->vulkanCommandPool.commandPool);
	renderDeviceVulkan->vulkanBuffer.createIndexBuffer(indices, renderDeviceVulkan->vulkanCommandPool.commandPool);
	renderDeviceVulkan->vulkanBuffer.createCombinedBuffer(vertices, indices, renderDeviceVulkan->vulkanCommandPool.commandPool);
	renderDeviceVulkan->vulkanBuffer.createUniformBuffers();

	renderDeviceVulkan->createDescriptorSetLayout();
	renderDeviceVulkan->createDescriptorPool();
	renderDeviceVulkan->createDescriptorSets(renderDeviceVulkan->vulkanBuffer.uniformBuffers);

	renderDeviceVulkan->pipeline.create();
	renderDeviceVulkan->pipeline.createGraphicsPipeline(renderDeviceVulkan->descriptorSetLayout, renderDeviceVulkan->swapchain.renderPass, sizeof(pushConstantData));
	renderDeviceVulkan->swapchain.createFramebuffers();
}

void Demo::mainLoop() {
	bool keyPressed = false;

	while (isRunning) {
		appWindow->onUpdate();

		camera.onUpdate();
		camera.processInput();

		bool action = AppWindow::isKeyPressed(KEY_1);
		if (action && !keyPressed) {
			pushConstantData.flag = !pushConstantData.flag;
			keyPressed = true;
		}
		else if (!action) {
			keyPressed = false;
		}

		drawFrame();

		guiManager->end();
	}
	vkDeviceWaitIdle(renderDeviceVulkan->device.device);
}

void Demo::cleanup() {
	destroyGuiDescriptorPool();
	renderDeviceVulkan->onClose();
	appWindow->onClose();
}

void Demo::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;					// Optional
	beginInfo.pInheritanceInfo = nullptr;	// Optional

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
	assert(imageIndex < renderDeviceVulkan->swapchain.swapChainFramebuffers.size() && "imageIndex out of range of framebuffers");
	
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderDeviceVulkan->swapchain.renderPass;
	renderPassInfo.framebuffer = renderDeviceVulkan->swapchain.swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = renderDeviceVulkan->swapchain.swapChainExtent;


	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	//basic draw commands
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderDeviceVulkan->pipeline.graphicsPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)renderDeviceVulkan->swapchain.swapChainExtent.width;
	viewport.height = (float)renderDeviceVulkan->swapchain.swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = renderDeviceVulkan->swapchain.swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// issue draw
	VkBuffer vertexBuffers[] = { renderDeviceVulkan->vulkanBuffer.vertexBuffer };
	VkDeviceSize offsets[] = { 0 };

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, renderDeviceVulkan->vulkanBuffer.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		renderDeviceVulkan->pipeline.pipelineLayout,
		0,
		1,
		&renderDeviceVulkan->descriptorSets[renderDeviceVulkan->currentFrame],
		0,
		nullptr
	);

	vkCmdPushConstants(
		commandBuffer,
		renderDeviceVulkan->pipeline.pipelineLayout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0, // offset
		sizeof(pushConstantData),
		&pushConstantData
	);

	vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
	//vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

	guiManager->start();
	guiManager->render(commandBuffer);


	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void Demo::drawFrame()
{
	//renderDeviceVulkan->beginFrame();

	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	VulkanDevice::UniformBufferObject ubo{};
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.proj[1][1] *= -1;

	//renderDeviceVulkan->vulkanBuffer.updateUniformBuffer(renderDeviceVulkan->currentFrame, ubo);

	//renderDeviceVulkan->render();
	//recordCommandBuffer(
	//	renderDeviceVulkan->vulkanCommandPool.commandBuffers[renderDeviceVulkan->currentFrame], 
	//	renderDeviceVulkan->imageIndex
	//);

	//renderDeviceVulkan->endFrame();

	vkWaitForFences(renderDeviceVulkan->device.device, 1, &renderDeviceVulkan->swapchain.inFlightFences[renderDeviceVulkan->currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(
		renderDeviceVulkan->device.device,
		renderDeviceVulkan->swapchain.swapChain,
		UINT64_MAX,
		renderDeviceVulkan->swapchain.imageAvailableSemaphores[renderDeviceVulkan->currentFrame],
		VK_NULL_HANDLE,
		&imageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		renderDeviceVulkan->swapchain.framebufferResized = false;
		renderDeviceVulkan->swapchain.recreateSwapchain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	renderDeviceVulkan->vulkanBuffer.updateUniformBuffer(renderDeviceVulkan->currentFrame, ubo);

	vkResetFences(renderDeviceVulkan->device.device, 1, &renderDeviceVulkan->swapchain.inFlightFences[renderDeviceVulkan->currentFrame]);
	vkResetCommandBuffer(renderDeviceVulkan->vulkanCommandPool.commandBuffers[renderDeviceVulkan->currentFrame], 0);
	recordCommandBuffer(renderDeviceVulkan->vulkanCommandPool.commandBuffers[renderDeviceVulkan->currentFrame], imageIndex);


	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { renderDeviceVulkan->swapchain.imageAvailableSemaphores[renderDeviceVulkan->currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &renderDeviceVulkan->vulkanCommandPool.commandBuffers[renderDeviceVulkan->currentFrame];

	VkSemaphore signalSemaphores[] = { renderDeviceVulkan->swapchain.renderFinishedSemaphores[imageIndex] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(renderDeviceVulkan->device.graphicsQueue, 1, &submitInfo, renderDeviceVulkan->swapchain.inFlightFences[renderDeviceVulkan->currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { renderDeviceVulkan->swapchain.swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(renderDeviceVulkan->device.presentQueue, &presentInfo);


	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		renderDeviceVulkan->swapchain.recreateSwapchain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}

	renderDeviceVulkan->currentFrame = (renderDeviceVulkan->currentFrame + 1) % renderDeviceVulkan->swapchain.MAX_FRAMES_IN_FLIGHT;
}

void Demo::createGuiDescriptorPool()
{
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
	pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkResult result = vkCreateDescriptorPool(renderDeviceVulkan->device.device, &pool_info, nullptr, &guiDescriptorPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool for ImGui");
	}
}

void Demo::destroyGuiDescriptorPool()
{
	vkDestroyDescriptorPool(renderDeviceVulkan->device.device, guiDescriptorPool, nullptr);
}

void Demo::initGuiContext()
{
	auto queueIndices = renderDeviceVulkan->device.findQueueFamilies(renderDeviceVulkan->device.physicalDevice);
	if (!queueIndices.graphicsFamily.has_value()) {
		throw std::runtime_error("Graphics queue family not found");
	}

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.ApiVersion = VK_API_VERSION_1_4;	// VkApplicationInfo::apiVersion
	init_info.Instance = renderDeviceVulkan->device.instance;
	init_info.PhysicalDevice = renderDeviceVulkan->device.physicalDevice;
	init_info.Device = renderDeviceVulkan->device.device;
	init_info.QueueFamily = queueIndices.graphicsFamily.value();
	init_info.Queue = renderDeviceVulkan->device.graphicsQueue;
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = guiDescriptorPool;
	init_info.MinImageCount = VulkanSwapChain::MAX_FRAMES_IN_FLIGHT;
	init_info.ImageCount = renderDeviceVulkan->swapchain.swapChainImages.size();
	init_info.Allocator = VK_NULL_HANDLE;
	init_info.PipelineInfoMain.RenderPass = renderDeviceVulkan->swapchain.renderPass;
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.CheckVkResultFn = [](VkResult err) { if (err != VK_SUCCESS) abort(); };

	ImGui_ImplGlfw_InitForVulkan(windowHandle, true);
	ImGui_ImplVulkan_Init(&init_info);
}

