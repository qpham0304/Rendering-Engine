#include "TextureManagerVulkan.h"
#include <stb/stb_image.h>
#include <vulkan/vulkan.h>
#include "core/features/ServiceLocator.h"
#include "graphics/framework/vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "logging/Logger.h"

TextureManagerVulkan::TextureManagerVulkan(std::string serviceName)
	: TextureManager(serviceName), renderDeviceVulkan(nullptr)
{

}

TextureManagerVulkan::~TextureManagerVulkan()
{

}

int TextureManagerVulkan::init(WindowConfig config)
{
	Service::init(config);

	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = static_cast<RenderDeviceVulkan*>(&device);
	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	vulkanBufferManager = static_cast<BufferManagerVulkan*>(&bufferManager);

	return 0;
}

int TextureManagerVulkan::onClose()
{
    WriteLock lock = _lockWrite();
	for (auto& [id, texture] : m_textures) {
		static_cast<TextureVulkan*>(texture.get())->destroy(renderDeviceVulkan->device);
	}
	m_textures.clear();
	m_textureData.clear();
	return 0;
}

void TextureManagerVulkan::destroy(uint32_t id)
{
	if (m_textures.find(id) == m_textures.end()) {
		throw std::runtime_error("texture not found");
	}
	getTexture(id)->destroy(renderDeviceVulkan->device);
	m_textures.erase(id);
}

uint32_t TextureManagerVulkan::loadTexture(std::string_view path)
{
	if (m_textureData.find(path.data()) != m_textureData.end()) {
		return m_textureData[path.data()];
	}

	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		std::string message = "TextureManagerVulkan::loadTexture: failed to load texture image ";
		throw std::runtime_error(message + path.data());
	}

	VkDeviceMemory stagingBufferMemory;
	VkBuffer stagingBuffer;

	vulkanBufferManager->createBuffer(
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vkMapMemory(renderDeviceVulkan->device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(renderDeviceVulkan->device, stagingBufferMemory);

	stbi_image_free(pixels);

	
	std::shared_ptr<TextureVulkan> texture = std::make_shared<TextureVulkan>(m_ids);
	m_textures[m_ids] = texture;
	m_textureData[path.data()] = m_ids;

	createImage(
		texWidth,
		texHeight,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture->textureImage,
		texture->textureImageMemory
	);

	transitionImageLayout(texture->textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, texture->textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	transitionImageLayout(texture->textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(renderDeviceVulkan->device, stagingBuffer, nullptr);
	vkFreeMemory(renderDeviceVulkan->device, stagingBufferMemory, nullptr);

	texture->textureImageView = createImageView(texture->textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	createTextureSampler(texture->textureSampler);
	m_logger->debug("Texture loaded {}, id: {}", path.data(), static_cast<uint32_t>(m_ids.load()));

	return _assignID();
}

TextureVulkan* TextureManagerVulkan::getTexture(uint32_t id)
{
	return dynamic_cast<TextureVulkan*>(TextureManager::getTexture(id));
}

void TextureManagerVulkan::createImage(
	uint32_t width, 
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

	if (vkCreateImage(renderDeviceVulkan->device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}


	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(renderDeviceVulkan->device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = vulkanBufferManager->findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(renderDeviceVulkan->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(renderDeviceVulkan->device, image, imageMemory, 0);
}

VkImageView TextureManagerVulkan::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
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
	if (vkCreateImageView(renderDeviceVulkan->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image view!");
	}

	return imageView;
}

void TextureManagerVulkan::createTextureSampler(VkSampler& textureSampler)
{
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(renderDeviceVulkan->device.physicalDevice, &properties);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(renderDeviceVulkan->device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void TextureManagerVulkan::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	barrier.dstAccessMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED 
		&& newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 
		&& newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	renderDeviceVulkan->commandPool.endSingleTimeCommand(commandBuffer);
}

void TextureManagerVulkan::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	renderDeviceVulkan->commandPool.endSingleTimeCommand(commandBuffer);
}

uint32_t TextureManagerVulkan::createDepthTexture() {
	
	std::shared_ptr<TextureVulkan> texture = std::make_shared<TextureVulkan>(m_ids);
	m_textures[m_ids] = texture;

	VkFormat depthFormat = findDepthFormat();

	createImage(
		renderDeviceVulkan->swapchain.swapChainExtent.width, 
		renderDeviceVulkan->swapchain.swapChainExtent.height, 
		depthFormat, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture->textureImage,
		texture->textureImageMemory
	);

	texture->textureImageView = createImageView(texture->textureImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	return _assignID();
}


VkFormat TextureManagerVulkan::findDepthFormat() {
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

VkFormat TextureManagerVulkan::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(renderDeviceVulkan->device.physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}
