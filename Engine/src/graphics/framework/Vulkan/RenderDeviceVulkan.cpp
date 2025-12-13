#include "RenderDeviceVulkan.h"
#include "../../src/core/features/ServiceLocator.h"
#include "../../src/Logging/Logger.h"
#include "resources/Textures/TextureVulkan.h"
#include <stb/stb_image.h>

RenderDeviceVulkan::RenderDeviceVulkan()
	: RenderDevice("RenderDeviceVulkan"),
	swapchain(device, *this),
	pipeline(device),
	vulkanBufferManager(),
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

	vulkanBufferManager.init();
	vulkanBufferManager.createUniformBuffers(sizeof(VulkanDevice::UniformBufferObject));

	createDepthResources();

	createDescriptorSetLayout();
	createDescriptorPool();

	pipeline.create();
	swapchain.createFramebuffers();
	pipeline.createGraphicsPipeline(descriptorSetLayout, swapchain.renderPass, pushConstantRange);


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

	vkDestroyDescriptorPool(device.device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device.device, descriptorSetLayout, nullptr);

	vkDestroyDescriptorPool(device.device, imguiDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device.device, imguiDescriptorSetLayout, nullptr);

	vulkanBufferManager.onClose();
	pipeline.destroy();
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

void RenderDeviceVulkan::createDescriptorSetLayout() {
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

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}


void RenderDeviceVulkan::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(device.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void RenderDeviceVulkan::createDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}
	
	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*vulkanBufferManager.uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(VulkanDevice::UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = texture->textureImageView;
		imageInfo.sampler = texture->textureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

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

void RenderDeviceVulkan::createTextureViewDescriptorSet()
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &samplerLayoutBinding;

	vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &imguiDescriptorSetLayout);

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiDescriptorPool);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = imguiDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &imguiDescriptorSetLayout;

	vkAllocateDescriptorSets(device, &allocInfo, &imguiTextureDescriptorSet);

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

	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
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

void RenderDeviceVulkan::setPushConstantRange(uint32_t range)
{
	pushConstantRange = range;
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
