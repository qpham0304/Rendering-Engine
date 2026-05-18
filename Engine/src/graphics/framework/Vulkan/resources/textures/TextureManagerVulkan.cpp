#include "TextureManagerVulkan.h"
#include <stb/stb_image.h>
#include <vulkan/vulkan.h>
#include "core/features/ServiceLocator.h"
#include "graphics/framework/vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/vulkan/core/VulkanDevice.h"
#include "graphics/framework/vulkan/core/VulkanUtils.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/resources/descriptors/DescriptorManagerVulkan.h"
#include "logging/Logger.h"
#include "core/events/EventManager.h"

TextureManagerVulkan::TextureManagerVulkan(std::string serviceName)
	: TextureManager(serviceName), renderDeviceVulkan(nullptr)
{

}

TextureManagerVulkan::~TextureManagerVulkan()
{

}

bool TextureManagerVulkan::init(WindowConfig config)
{
	Service::init(config);

	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = static_cast<RenderDeviceVulkan*>(&device);

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	vulkanBufferManager = static_cast<BufferManagerVulkan*>(&bufferManager);

	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = &static_cast<DescriptorManagerVulkan&>(descriptorManager);

	if(!(renderDeviceVulkan && vulkanBufferManager)){
		return false;
	}

	_createInspectorDescriptorBind();
	_createBindlessDescriptor();

	return true;
}

bool TextureManagerVulkan::onClose()
{
    WriteLock lock = _lockWrite();
	for (auto& [id, texture] : m_textures) {
		static_cast<TextureVulkan*>(texture.get())->destroy(renderDeviceVulkan->device);
	}
	
	m_textures.clear();
	m_textureData.clear();

	return true;
}

void TextureManagerVulkan::destroy(uint32_t id)
{
	auto it = m_textures.find(id);
	if (it == m_textures.end()) {
		m_logger->warn("Texture not found id:{}", id);
		return;
	}
	
	TextureVulkan* texture = getTexture(id);
	std::string path = texture->m_path;
	texture->destroy(renderDeviceVulkan->device);
	m_textures[id] = nullptr;

	auto it2 = m_textureData.find(path);
	if(it2 != m_textureData.end()){
		m_textureData.erase(path);
	}

	auto it3 = textureIDs.find(id);
	if(it3 != textureIDs.end()) {
		textureIDs.erase(id);
	}

	
	m_textures.erase(id);
}

uint32_t TextureManagerVulkan::loadTexture(std::string_view path, uint32_t mipLevels, bool isDataTexture)
{
	if (m_textureData.find(path.data()) != m_textureData.end()) {
		return m_textureData.at(path.data());
	}

	_loadTexture(path, mipLevels, isDataTexture);

	return _assignID();
}

uint32_t TextureManagerVulkan::createTexture()
{
	m_textures[m_ids] = std::make_shared<TextureVulkan>(m_ids);

	return _assignID();
}

TextureVulkan* TextureManagerVulkan::getTexture(uint32_t id)
{
	return dynamic_cast<TextureVulkan*>(TextureManager::getTexture(id));
}

void* TextureManagerVulkan::inspectTexture(uint32_t id)
{
    TextureVulkan* textureVulkan = getTexture(id);
    if(!textureVulkan || textureVulkan->textureImageView == VK_NULL_HANDLE) {
		return nullptr;
	}

    if(textureIDs.find(id) != textureIDs.end()) {
		VkDescriptorSet descriptorSet = descriptorManagerVulkan->getDescriptorSet(textureIDs[id])[0];
		return (void*)descriptorSet;
    } 

    uint32_t setID = descriptorManagerVulkan->createSets(inspectorLayoutID, inspectorPoolID, 1);
	textureIDs[id] = setID;

    VkDescriptorSet descriptorSet = descriptorManagerVulkan->getDescriptorSet(setID)[0];

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureVulkan->textureImageView;
    imageInfo.sampler = textureVulkan->textureSampler;

    std::vector<VkWriteDescriptorSet> writes{};
    descriptorManagerVulkan->writeImage(&writes, descriptorSet, 0, imageInfo);
    descriptorManagerVulkan->updateDescriptorSets(&writes);

    return (void*)descriptorSet;
}

uint32_t TextureManagerVulkan::getInspectorLayout() {
	return inspectorLayoutID;
} 

void TextureManagerVulkan::_loadTexture(std::string_view path, uint32_t mipLevels, bool isDataTexture)
{
	stbi_set_flip_vertically_on_load(true);
	bool isHDR = stbi_is_hdr(path.data());
	VkFormat format;
	uint32_t bytesPerChannel;
	void* pixels;

	int texWidth, texHeight, texChannels;

	if (isHDR) {
		pixels = stbi_loadf(path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		format = VK_FORMAT_R32G32B32A32_SFLOAT;
		bytesPerChannel = sizeof(float);
	} else {
		pixels = stbi_load(path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		format = isDataTexture ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
		bytesPerChannel = sizeof(uint8_t);
	}

	VkDeviceSize imageSize = texWidth * texHeight * 4 * bytesPerChannel;

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
	texture->m_path = path;
	texture->m_width = texWidth;
	texture->m_height = texHeight;
	m_textures[m_ids] = texture;
	m_textureData[path.data()] = m_ids;

	createImage(
		texWidth,
		texHeight,
		format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture->textureImage,
		texture->textureImageMemory,
		mipLevels,
		renderDeviceVulkan->device
	);

	transitionImageLayout(
		texture->textureImage,
		format,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		mipLevels,
		renderDeviceVulkan
	);
	copyBufferToImage(stagingBuffer, texture->textureImage, texWidth, texHeight, renderDeviceVulkan);
	generateMipmaps(texture->textureImage, format, texWidth, texHeight, mipLevels, renderDeviceVulkan);

	vkDestroyBuffer(renderDeviceVulkan->device, stagingBuffer, nullptr);
	vkFreeMemory(renderDeviceVulkan->device, stagingBufferMemory, nullptr);

	createImageView(
		texture->textureImage,
		texture->textureImageView,
		format,
		VK_IMAGE_ASPECT_COLOR_BIT,
		mipLevels,
		renderDeviceVulkan->device
	);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	if (isDataTexture) {
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // No blending between mips
    } else {
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
    }
	
	if (isHDR) {
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	} else {
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
	createTextureSampler(texture->textureSampler, renderDeviceVulkan->device, samplerInfo);
	m_logger->debug("Texture loaded {}, id: {}", path.data(), static_cast<uint32_t>(m_ids.load()));

}

void TextureManagerVulkan::_createInspectorDescriptorBind()
{
	// add more if requires more potential crash if exceed the max number
	const uint32_t MAX_NUM_SETS = 1000;
	
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> bindings = { samplerLayoutBinding };
	inspectorLayoutID = descriptorManagerVulkan->createLayout(bindings);

	std::vector<VkDescriptorPoolSize> poolSizes = { 
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } 
	};
	inspectorPoolID = descriptorManagerVulkan->createPool(poolSizes, MAX_NUM_SETS);
}

void TextureManagerVulkan::_createBindlessDescriptor()
{
    VkDescriptorSetLayoutBinding bindlessBinding{};
    bindlessBinding.binding = 0;
    bindlessBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindlessBinding.descriptorCount = 10000;
    bindlessBinding.stageFlags = VK_SHADER_STAGE_ALL;

    globalBindlessLayoutID = descriptorManagerVulkan->createLayout(
        { bindlessBinding }, 
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
    );

    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10000 };
    globalBindlessPoolID = descriptorManagerVulkan->createPool(
        { poolSize }, 
        1,
        VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
    );

    globalBindlessSetID = descriptorManagerVulkan->createSets(globalBindlessLayoutID, globalBindlessPoolID, 1);
}

void TextureManagerVulkan::registerTextureSampler(uint32_t textureID)
{
    TextureVulkan* tex = getTexture(textureID);
    VkDescriptorSet globalSet = descriptorManagerVulkan->getDescriptorSet(globalBindlessSetID)[0];

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = tex->textureImageView;
    imageInfo.sampler = tex->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeImage(&writes, globalSet, 0, imageInfo, textureID);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void TextureManagerVulkan::registerTextureStorage(uint32_t textureID, VkImageLayout layout)
{
    TextureVulkan* tex = getTexture(textureID);
    VkDescriptorSet globalSet = descriptorManagerVulkan->getDescriptorSet(globalBindlessSetID)[0];

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = layout;
    imageInfo.imageView = tex->textureImageView;
    imageInfo.sampler = tex->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, globalSet, 0, imageInfo, textureID);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

uint32_t TextureManagerVulkan::getBindlessTextureLayout()
{
	return globalBindlessLayoutID;
}

uint32_t TextureManagerVulkan::getBindlessSet()
{
    return globalBindlessSetID;
}

void TextureManagerVulkan::createImage(
	uint32_t width, 
	uint32_t height, 
	VkFormat format, 
	VkImageTiling tiling, 
	VkImageUsageFlags usage, 
	VkMemoryPropertyFlags properties, 
	VkImage& image, 
	VkDeviceMemory& imageMemory,
	uint32_t mipLevels,
	const VulkanDevice& device
) {
	createImage(
		width,
		height,
		format,
		tiling,
		usage,
		properties,
		image,
		imageMemory,
		mipLevels,
		1,
		0,
		device
	);
}

void TextureManagerVulkan::createImage(
    uint32_t width, 
    uint32_t height, 
    VkFormat format, 
    VkImageTiling tiling, 
    VkImageUsageFlags usage, 
    VkMemoryPropertyFlags properties, 
    VkImage& image, 
    VkDeviceMemory& imageMemory,
    uint32_t mipLevels,
    uint32_t arrayLayers,          
    VkImageCreateFlags flags,      
    const VulkanDevice& device
) {
	VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage; 
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = flags;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }
	

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(device.getPhysicalDevice(), memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(device, image, imageMemory, 0);
}

void TextureManagerVulkan::createImageView(
	VkImage &image,
	VkImageView &imageView,
	VkFormat format,
	VkImageAspectFlags aspectFlags,
	uint32_t mipLevels,
	VulkanDevice &device
) {
	createImageView(
		image, 
		imageView, 
		format, 
		aspectFlags, 
        mipLevels, 
		0, 
		1, 
		VK_IMAGE_VIEW_TYPE_2D, 
		device
    );
}

void TextureManagerVulkan::createImageView(
    VkImage image, 
    VkImageView& imageView, 
    VkFormat format, 
    VkImageAspectFlags aspectFlags, 
    uint32_t mipLevels, 
    uint32_t baseMipLevel,
    uint32_t layerCount, 
    VkImageViewType viewType,
    const VulkanDevice& device
) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = viewType;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = layerCount;

    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
}

VkSamplerCreateInfo TextureManagerVulkan::createLinearSampler(VulkanDevice &device)
{
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(device.getPhysicalDevice(), &properties);

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

	return samplerInfo;
}

void TextureManagerVulkan::createTextureSampler(VkSampler& textureSampler, VulkanDevice& device, VkSamplerCreateInfo samplerInfo)
{
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(device.getPhysicalDevice(), &properties);

	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	
	if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

// NOTE: transition only support color bit at the moment
// depth transition need to create it own barrier
void TextureManagerVulkan::transitionImageLayout(
	VkCommandBuffer cmd,
	VkImage image,
	VkFormat format,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	uint32_t mipLevels,
	uint32_t layerCount,
	RenderDeviceVulkan* renderDeviceVulkan	//TODO: this is not used
){
	VkImageAspectFlags aspect = 0;
	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM) {
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	} else {
		aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layerCount;
	barrier.srcAccessMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	barrier.dstAccessMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; 
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} 
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
	{
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; 
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		
		// Check if it's a depth format to set the correct aspect mask
		if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		} else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		} else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		} else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		cmd,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

}


void TextureManagerVulkan::transitionImageLayout(
	VkImage image,
	VkFormat format,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	uint32_t mipLevels,
	RenderDeviceVulkan* renderDeviceVulkan
){
	VkCommandBuffer commandBuffer = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

	transitionImageLayout(
		commandBuffer,
		image,
		format,
		oldLayout,
		newLayout,
		mipLevels,
		1,
		renderDeviceVulkan
	);

	renderDeviceVulkan->commandPool.endSingleTimeCommand(commandBuffer);
}


void TextureManagerVulkan::copyBufferToImage(
	VkBuffer buffer,
	VkImage image,
	uint32_t width,
	uint32_t height,
	RenderDeviceVulkan* renderDeviceVulkan
) {
	VkCommandBuffer commandBuffer = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;	//NOTE: currently set layer count to 1

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

uint32_t TextureManagerVulkan::createTexture(TextureConfig textureConfig, TextureSamplerConfig samplerConfig)
{
	std::shared_ptr<TextureVulkan> texture = std::make_shared<TextureVulkan>(m_ids);
	m_textures[m_ids] = texture;

	TextureManagerVulkan::createImage(
		textureConfig.width,
		textureConfig.height,
		textureConfig.format,
		textureConfig.tiling,
		textureConfig.usage,
		textureConfig.properties,
		texture->textureImage,
		texture->textureImageMemory,
		textureConfig.mipLevels,
		renderDeviceVulkan->device
	);

	TextureManagerVulkan::createImageView(
		texture->textureImage,
		texture->textureImageView,
		textureConfig.format,
		textureConfig.aspectBits,
		textureConfig.mipLevels,
		renderDeviceVulkan->device
	);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = samplerConfig.filter;
	samplerInfo.minFilter = samplerConfig.filter;
	samplerInfo.addressModeU = samplerConfig.addressMode;
	samplerInfo.addressModeV = samplerConfig.addressMode;
	samplerInfo.addressModeW = samplerConfig.addressMode;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = samplerConfig.mipmapMode;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;

	TextureManagerVulkan::createTextureSampler(
		texture->textureSampler, 
		renderDeviceVulkan->device,
		samplerInfo
	);

	texture->init(textureConfig, samplerConfig);

	auto cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
	texture->transitImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);
	
    return _assignID();
}

uint32_t TextureManagerVulkan::createDepthTexture(uint32_t width, uint32_t height, uint32_t miplevels) {
	
	m_textures[m_ids] = std::make_shared<TextureVulkan>(m_ids);
	TextureVulkan* texture = static_cast<TextureVulkan*>(m_textures[m_ids].get());

	VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

	TextureManagerVulkan::createImage(
		width,
		height,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture->textureImage,
		texture->textureImageMemory,
		miplevels,
		renderDeviceVulkan->device
	);
	
	TextureManagerVulkan::createImageView(
		texture->textureImage,
		texture->textureImageView,
		depthFormat,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		miplevels,
		renderDeviceVulkan->device
	);

	return _assignID();
}


VkFormat TextureManagerVulkan::findDepthFormat(const VulkanDevice& device) {
	return TextureManagerVulkan::findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
		device
	);
}

VkFormat TextureManagerVulkan::findSupportedFormat(
	const std::vector<VkFormat>& candidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags features,
	const VulkanDevice& device
) {
	for (VkFormat format : candidates
	) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(device.getPhysicalDevice(), format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

void TextureManagerVulkan::generateMipmaps(
	VkImage image,
	VkFormat imageFormat,
	int32_t texWidth,
	int32_t texHeight,
	uint32_t mipLevels,
	RenderDeviceVulkan* renderDeviceVulkan
) {
	VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(renderDeviceVulkan->device.getPhysicalDevice(), imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

    VkCommandBuffer commandBuffer = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        // Transition level i-1 to TRANSFER_SRC_OPTIMAL
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
			commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 
			0, nullptr, 
			1, &barrier
		);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(
			commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
			VK_FILTER_LINEAR
		);

        // transition level i-1 to SHADER_READ_ONLY_OPTIMAL
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
			commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier
		);

        if (mipWidth > 1) {
			mipWidth /= 2;
		}
		
        if (mipHeight > 1) {
			mipHeight /= 2;
		}
    }

    // transition the last mip level
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
		commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr, 0, nullptr, 1, &barrier
	);

    renderDeviceVulkan->commandPool.endSingleTimeCommand(commandBuffer);
}

void TextureManagerVulkan::createBarrier(
	VkCommandBuffer cmd,
	VkImage image,
	VkAccessFlags srcAccess,
	VkAccessFlags dstAccess,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	VkPipelineStageFlags srcStage,
	VkPipelineStageFlags dstStage
) {
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
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;

	vkCmdPipelineBarrier(
		cmd,
		srcStage, dstStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

// only copy image of the screen size at the moment
// only support copy the same image type
// TODO: support down scaled imaged copy and down sample instead of swapchain size
void TextureManagerVulkan::copyImage(
	VkCommandBuffer cmd, 
	TextureVulkan *srcImage, 
	TextureVulkan *dstImage, 
	VkFormat format, 
	VkImageAspectFlags aspect,
	RenderDeviceVulkan *renderDeviceVulkan
){
    TextureManagerVulkan::transitionImageLayout(
        cmd, srcImage->textureImage, format,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(
        cmd, dstImage->textureImage, format,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1, renderDeviceVulkan);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { aspect, 0, 0, 1 };
    copyRegion.dstSubresource = { aspect, 0, 0, 1 };
    copyRegion.extent = { 	
        renderDeviceVulkan->swapchain.swapChainExtent.width, 
        renderDeviceVulkan->swapchain.swapChainExtent.height, 
        1 
    };

    vkCmdCopyImage(
        cmd, srcImage->textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage->textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion
    );

    TextureManagerVulkan::transitionImageLayout(
        cmd, srcImage->textureImage, format,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(
        cmd, dstImage->textureImage, format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

}

void TextureManagerVulkan::copyImage(
	VkCommandBuffer cmd,
	TextureVulkan *srcImage,
	TextureVulkan *dstImage,
	VkFormat format,
	VkImageAspectFlags aspect,
	uint32_t imageWidth,
	uint32_t imageHeight,
	RenderDeviceVulkan *renderDeviceVulkan
) {
    TextureManagerVulkan::transitionImageLayout(
        cmd, srcImage->textureImage, format,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(
        cmd, dstImage->textureImage, format,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1, renderDeviceVulkan);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { aspect, 0, 0, 1 };
    copyRegion.dstSubresource = { aspect, 0, 0, 1 };
    copyRegion.extent = { imageWidth, imageHeight, 1 };

    vkCmdCopyImage(
        cmd, srcImage->textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage->textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion
    );

    TextureManagerVulkan::transitionImageLayout(
        cmd, srcImage->textureImage, format,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(
        cmd, dstImage->textureImage, format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

}
