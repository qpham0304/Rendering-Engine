#include "FollowDemo.h"
#include <cstring>
#include <cassert>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"
#include "../../src/window/AppWindow.h"
#include "../../src/core/events/EventManager.h"

//Demo* Demo::demoInstance = nullptr;
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
	renderDeviceVulkan->device.createInstance();
	renderDeviceVulkan->device.setupDebugMessenger();
	renderDeviceVulkan->device.createSurface();
	renderDeviceVulkan->device.selectPhysicalDevice();
	renderDeviceVulkan->device.createLogicalDevice();
	device = renderDeviceVulkan->device.device;

	renderDeviceVulkan->swapchain.create();
	swapChain = renderDeviceVulkan->swapchain.swapChain;
	swapChainImages = renderDeviceVulkan->swapchain.swapChainImages;
	swapChainImageFormat = renderDeviceVulkan->swapchain.swapChainImageFormat;
	swapChainExtent = renderDeviceVulkan->swapchain.swapChainExtent;
	swapChainImageViews = renderDeviceVulkan->swapchain.swapChainImageViews;
	renderPass = renderDeviceVulkan->swapchain.renderPass;


	createDescriptorSetLayout();

	createDescriptorPool();

	renderDeviceVulkan->pipeline.create();
	renderDeviceVulkan->pipeline.createGraphicsPipeline(descriptorSetLayout, renderPass, sizeof(pushConstantData));


	renderDeviceVulkan->swapchain.createFramebuffers();
	//swapChainFramebuffers = renderDeviceVulkan->swapchain.swapChainFramebuffers;

	createCommandPool();
	createCommandBuffers();

	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createCombinedBuffer();

	createDescriptorSets();


	createSyncObject();
}

void Demo::mainLoop() {
	bool keyPressed = false;

	while (isRunning) {
		appWindow->onUpdate();

		camera.onUpdate();
		camera.processInput();

		//orbitCamera.onUpdate();
		//orbitCamera.processKeyboard(window); // optional pan
		//orbitCamera.mouseControl(window);    // orbit rotation
		
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
	vkDeviceWaitIdle(device);
}

void Demo::cleanup() {
	//vkDeviceWaitIdle(device); 
	//cleanupSwapChain();
	renderDeviceVulkan->swapchain.destroy();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(device, uniformBuffers[i], nullptr);
		vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
	}
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

	destroyGuiDescriptorPool();

	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
	vkDestroyBuffer(device, combinedBuffer, nullptr);
	vkFreeMemory(device, combinedBufferMemory, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyFence(device, inFlightFences[i], nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
	}

	for (size_t i = 0; i < swapChainImages.size(); i++) {
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
	}

	renderDeviceVulkan->pipeline.destroy();

	vkDestroyRenderPass(device, renderPass, nullptr);

	vkDestroyCommandPool(device, commandPool, nullptr);

	renderDeviceVulkan->device.destroy();
	appWindow->onClose();
}

void Demo::createCommandPool() {
	VulkanDevice::QueueFamilyIndices queueFamilyIndices = renderDeviceVulkan->device.findQueueFamilies(renderDeviceVulkan->device.physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}
}

void Demo::createVertexBuffer() {
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);


	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer,
		vertexBufferMemory
	);

	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Demo::createIndexBuffer()
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer,
		indexBufferMemory
	);

	copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Demo::createCombinedBuffer()
{
	VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
	VkDeviceSize wholeBufferSize = vertexBufferSize + indexBufferSize;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(
		wholeBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);
	memcpy(data, vertices.data(), (size_t)vertexBufferSize);
	memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), (size_t)vertexBufferSize);
	vkUnmapMemory(device, stagingBufferMemory);


	createBuffer(
		wholeBufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		combinedBuffer,
		combinedBufferMemory
	);

	copyBuffer(stagingBuffer, combinedBuffer, wholeBufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Demo::createCommandBuffers() {
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}
}

void Demo::createSyncObject() {
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(swapChainImages.size());
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}


	for (int i = 0; i < swapChainImages.size(); i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}
}

void Demo::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Demo::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(VulkanDevice::UniformBufferObject);

	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniformBuffers[i],
			uniformBuffersMemory[i]
		);

		vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}
}

void Demo::updateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	VulkanDevice::UniformBufferObject ubo{};
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	//ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();

	//ubo.model = glm::mat4(1.0f);
	//ubo.view = orbitCamera.getViewMatrix();
	//ubo.proj = orbitCamera.getProjectionMatrix();
	ubo.proj[1][1] *= -1;

	memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));	// update every frame

}

void Demo::createDescriptorPool()
{
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void Demo::createDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(VulkanDevice::UniformBufferObject);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;
		descriptorWrite.pImageInfo = nullptr;		// Optional
		descriptorWrite.pTexelBufferView = nullptr; // Optional
		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}

}

void Demo::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
	assert(imageIndex < renderDeviceVulkan->swapchain.swapChainFramebuffers.size() && "imageIndex out of range of framebuffers");
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = renderDeviceVulkan->swapchain.swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;


	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	//basic draw commands
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderDeviceVulkan->pipeline.graphicsPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// issue draw
	VkBuffer vertexBuffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

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
		0, // offset
		sizeof(pushConstantData),
		&pushConstantData
	);

	vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
	//vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

	guiManager->start();
	guiManager->render();
	renderGui(commandBuffer);


	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void Demo::drawFrame()
{
	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(
		device,
		swapChain,
		UINT64_MAX,
		imageAvailableSemaphores[currentFrame],
		VK_NULL_HANDLE,
		&imageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
		framebufferResized = false;
		renderDeviceVulkan->swapchain.recreateSwapchain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	updateUniformBuffer(currentFrame);

	vkResetFences(device, 1, &inFlightFences[currentFrame]);
	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	recordCommandBuffer(commandBuffers[currentFrame], imageIndex);


	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(renderDeviceVulkan->device.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapChain };
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

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t Demo::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(renderDeviceVulkan->device.physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void Demo::createBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkBuffer& buffer,
	VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Demo::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(renderDeviceVulkan->device.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(renderDeviceVulkan->device.graphicsQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
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

	VkResult result = vkCreateDescriptorPool(device, &pool_info, nullptr, &guiDescriptorPool);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool for ImGui");
	}
}

void Demo::destroyGuiDescriptorPool()
{
	vkDestroyDescriptorPool(device, guiDescriptorPool, nullptr);
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
	init_info.Device = device;
	init_info.QueueFamily = queueIndices.graphicsFamily.value();
	init_info.Queue = renderDeviceVulkan->device.graphicsQueue;
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = guiDescriptorPool;
	init_info.MinImageCount = Demo::MAX_FRAMES_IN_FLIGHT;
	init_info.ImageCount = swapChainImages.size();
	init_info.Allocator = VK_NULL_HANDLE;
	init_info.PipelineInfoMain.RenderPass = renderPass;
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.CheckVkResultFn = [](VkResult err) { if (err != VK_SUCCESS) abort(); };

	ImGui_ImplGlfw_InitForVulkan(windowHandle, true);
	ImGui_ImplVulkan_Init(&init_info);
}

void Demo::renderGui(VkCommandBuffer commandBuffer)
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}
