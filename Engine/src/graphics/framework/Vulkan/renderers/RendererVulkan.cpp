#include "RendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "graphics/RenderDevice.h"
#include "graphics/framework/Vulkan/RenderDeviceVulkan.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

RendererVulkan::RendererVulkan()
{
}

RendererVulkan::~RendererVulkan()
{
}

void RendererVulkan::init()
{
	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);
	if (!renderDeviceVulkan) {
		throw std::runtime_error("Failed to accquire render device");
	}
	EventManager::getInstance().subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_1) {
			pushConstantData.flag = !pushConstantData.flag;
		}
	});

	pushConstantData.flag = false;
	pushConstantData.color = glm::vec3(1.0f, 1.0f, 0.0f);
	pushConstantData.range = glm::vec3(1.0f, 1.0f, 1.0f);
	pushConstantData.data = 0.1f;

	renderDeviceVulkan->setPushConstantRange(sizeof(PushConstantData));
	renderDeviceVulkan->init(AppWindow::getWindowConfig());
}

void RendererVulkan::beginFrame()
{
	renderDeviceVulkan->beginFrame();
}

void RendererVulkan::endFrame()
{
	renderDeviceVulkan->endFrame();
}

void RendererVulkan::render()
{
	renderDeviceVulkan->render();
	//beginRecording();
	//renderFunction();
	//endRecording();
}

void RendererVulkan::shutdown()
{

}


void RendererVulkan::addMesh()
{

}

void RendererVulkan::addModel(std::string_view path)
{

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

	vkCmdBindDescriptorSets(
		renderDeviceVulkan->commandPool.commandBuffers[renderDeviceVulkan->getCurrentFrame()],
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		renderDeviceVulkan->pipeline.pipelineLayout,
		0,
		1,
		&renderDeviceVulkan->descriptorSets[renderDeviceVulkan->getCurrentFrame()],
		0,
		nullptr
	);

	vkCmdPushConstants(
		renderDeviceVulkan->commandPool.commandBuffers[renderDeviceVulkan->getCurrentFrame()],
		renderDeviceVulkan->pipeline.pipelineLayout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0, // offset
		sizeof(PushConstantData),
		&pushConstantData
	);
}



void RendererVulkan::endRecording(void* cmdBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	vkCmdEndRenderPass(commandBuffer);

	renderDeviceVulkan->commandPool.endBuffer();
}
