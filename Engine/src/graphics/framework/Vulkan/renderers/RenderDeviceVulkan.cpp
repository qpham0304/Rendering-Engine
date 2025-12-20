#include "RenderDeviceVulkan.h"
#include "core/features/ServiceLocator.h"
#include "Logging/Logger.h"
#include "graphics/framework/vulkan/resources/Textures/TextureVulkan.h"

RenderDeviceVulkan::RenderDeviceVulkan()
	: RenderDevice("RenderDeviceVulkan"),
	swapchain(device, *this),
	pipeline(device),
	commandPool(device)
{

}

// everything else are destroyed explicitly except core resources
// vulkan device resources are the last to be destroyed automatically right before the app closed
RenderDeviceVulkan::~RenderDeviceVulkan()
{
	_cleanup();	
}


bool RenderDeviceVulkan::init(WindowConfig config)
{
	Service::init(config);

	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
	m_logger->info("Vulkan Render Device initialized");

	device.create();
	swapchain.create();
	commandPool.create();

	swapchain.createFramebuffers();

	return true;
}

// wait for other vulkan resources to be destroyed before device cleanup
bool RenderDeviceVulkan::onClose()
{
	waitIdle();
	return true;
}

void RenderDeviceVulkan::onUpdate()
{
	
}

void RenderDeviceVulkan::beginFrame()
{
	vkWaitForFences(device.device, 1, &swapchain.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	swapchain.accquireNextImage(currentFrame);
}

void RenderDeviceVulkan::render()
{
	vkResetFences(device.device, 1, &swapchain.inFlightFences[currentFrame]);
	vkResetCommandBuffer(commandPool.currentBuffer(), 0);
}

void RenderDeviceVulkan::endFrame()
{
	swapchain.submitAndPresent(currentFrame, commandPool.currentBuffer());

	currentFrame = (currentFrame + 1) % VulkanSwapChain::MAX_FRAMES_IN_FLIGHT;
}

void RenderDeviceVulkan::draw(uint32_t numIndicies, uint32_t numInstances, uint32_t offset)
{
	if (numInstances < 1) {
		return;
	}

	if (numInstances == 1) {
		commandPool.drawIndexed(numIndicies, offset);
	}
	else {
		commandPool.drawInstanced(numIndicies, numInstances, offset);
	}
}

const uint32_t& RenderDeviceVulkan::getCurrentFrameIndex() const
{
	return currentFrame;
}

const uint32_t& RenderDeviceVulkan::getImageIndex() const
{
	return swapchain.getImageIndex();
}

void* RenderDeviceVulkan::getNativeInstance()
{
	return static_cast<void*>(device.instance);
}

void* RenderDeviceVulkan::getNativeDevice()
{
	return static_cast<void*>(device.device);
}

void* RenderDeviceVulkan::getPhysicalDevice()
{
	return static_cast<void*>(device.physicalDevice);
}

Logger& RenderDeviceVulkan::Log() const
{
	return *m_logger;
}

uint16_t RenderDeviceVulkan::_assignID()
{
	return m_ids.fetch_add(1, std::memory_order_relaxed);
}

void RenderDeviceVulkan::_cleanup()
{
	swapchain.destroy();
	commandPool.destroy();
	device.destroy();
}


RenderDeviceVulkan::DeviceInfo RenderDeviceVulkan::getDeviceInfo() const
{
	auto queueIndices = device.findQueueFamilies(device.physicalDevice);
	if (!queueIndices.graphicsFamily.has_value()) {
		throw std::runtime_error("Graphics queue family not found");
	}

	DeviceInfo deviceInfo{};
	deviceInfo.apiVersion = VK_API_VERSION_1_4;
	deviceInfo.queueFamilyIndex = queueIndices.graphicsFamily.value();
	deviceInfo.queueHandle = device.graphicsQueue;
	deviceInfo.minImageCount = swapchain.MAX_FRAMES_IN_FLIGHT;
	deviceInfo.imageCount = swapchain.swapChainImages.size();

	return deviceInfo;
}

RenderDeviceVulkan::PipelineInfo RenderDeviceVulkan::getPipelineInfo() const
{
	PipelineInfo pipelineInfo{};
	pipelineInfo.pipelineFlags = 0;
	pipelineInfo.renderPass = swapchain.renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	return pipelineInfo;
}

bool RenderDeviceVulkan::hasStencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void RenderDeviceVulkan::setViewport()
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchain.swapChainExtent.width;
	viewport.height = (float)swapchain.swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandPool.currentBuffer(), 0, 1, &viewport);
}

void RenderDeviceVulkan::setScissor()
{
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchain.swapChainExtent;
	vkCmdSetScissor(commandPool.currentBuffer(), 0, 1, &scissor);
}

void RenderDeviceVulkan::waitIdle()
{
	vkDeviceWaitIdle(device);
}
