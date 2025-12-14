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


int RenderDeviceVulkan::init(WindowConfig config)
{
	Service::init(config);

	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
	m_logger->info("Vulkan Render Device initialized");

	device.create();
	swapchain.create();
	commandPool.create();

	createDepthResources();
	swapchain.createFramebuffers();

	return 0;
}

// wait for other vulkan resources to be destroyed before device cleanup
int RenderDeviceVulkan::onClose()
{
	waitIdle();
	return 0;
}

void RenderDeviceVulkan::onUpdate()
{
	throw std::runtime_error("beginFrame not implemented");
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

void RenderDeviceVulkan::draw(uint32_t numIndicies, uint32_t numInstances)
{
	if (numInstances < 1) {
		return;
	}

	if (numInstances == 1) {
		commandPool.drawIndexed(numIndicies);
	}
	else {
		commandPool.drawInstanced(numIndicies, numInstances);
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

VkImageView RenderDeviceVulkan::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image view!");
	}

	return imageView;
}

void RenderDeviceVulkan::createDepthResources()
{
	VkFormat depthFormat = findDepthFormat();

	createImage(swapchain.swapChainExtent.width,
		swapchain.swapChainExtent.height,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depthImage,
		depthImageMemory
	);
	depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void RenderDeviceVulkan::createImage(uint32_t width,
	uint32_t height,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkImage& image,
	VkDeviceMemory& imageMemory
) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(device.physicalDevice, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(device, image, imageMemory, 0);
}


VkFormat RenderDeviceVulkan::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(device.physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

bool RenderDeviceVulkan::hasStencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat RenderDeviceVulkan::findDepthFormat() {
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
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
