#pragma once

#include "core/resources/managers/TextureManager.h"
#include <vulkan/vulkan.h>
#include "TextureVulkan.h"

class RenderDeviceVulkan;
class BufferManagerVulkan;
class DescriptorManagerVulkan;
class VulkanDevice;

class TextureManagerVulkan : public TextureManager
{
public:
	struct TextureConfig {
		uint32_t width;
		uint32_t height; 
		VkFormat format; 
		VkImageUsageFlags usage; 
		VkImageAspectFlags aspect;
		uint32_t mipLevels;
	};

	static void createImage(
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
	);

	static void createImage(
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
	);

	static void createImageView(
		VkImage& image,
		VkImageView& imageView,
		VkFormat format,
		VkImageAspectFlags aspectFlags,
		uint32_t mipLevels,
		VulkanDevice& device
	);

	static void createImageView(
		VkImage image, 
		VkImageView& imageView, 
		VkFormat format, 
		VkImageAspectFlags aspectFlags, 
		uint32_t mipLevels, 
		uint32_t baseMipLevel,
		uint32_t layerCount,
		VkImageViewType viewType,
		const VulkanDevice& device
	);

	static void createTextureSampler(
		VkSampler& textureSampler,
		VulkanDevice& device
	);

	static void createTextureSampler(
		VkSampler& textureSampler,
		VulkanDevice& device,
		VkSamplerCreateInfo samplerInfo
	);

	static void transitionImageLayout(
		VkCommandBuffer cmd,
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		uint32_t mipLevels,
		uint32_t layerCount,
		RenderDeviceVulkan* renderDeviceVulkan
	);

	static void transitionImageLayout(
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		uint32_t mipLevels,
		RenderDeviceVulkan* renderDeviceVulkan
	);

	static void copyBufferToImage(
		VkBuffer buffer,
		VkImage image,
		uint32_t width,
		uint32_t height,
		RenderDeviceVulkan* renderDeviceVulkan
	);

	static VkFormat findDepthFormat(const VulkanDevice& device);

	static VkFormat findSupportedFormat(
		const std::vector<VkFormat>& candidates,
		VkImageTiling tiling,
		VkFormatFeatureFlags features,
		const VulkanDevice& device
	);

	static void generateMipmaps(
		VkImage image,
		VkFormat imageFormat,
		int32_t texWidth,
		int32_t texHeight,
		uint32_t mipLevels,
		RenderDeviceVulkan* renderDeviceVulkan
	);

	static void createBarrier(
		VkCommandBuffer cmd,
		VkImage image,
		VkAccessFlags srcAccess,
		VkAccessFlags dstAccess,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		VkPipelineStageFlags srcStage,
		VkPipelineStageFlags dstStage
	);

	static void copyImage(
		VkCommandBuffer cmd,
		TextureVulkan* srcImage,
		TextureVulkan* dstImage,
		VkFormat format,
		VkImageAspectFlags aspect,
		RenderDeviceVulkan* renderDeviceVulkan
	);

public:
	TextureManagerVulkan(std::string serviceName = "TextureManagerVulkan");	
	~TextureManagerVulkan();

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual uint32_t loadTexture(std::string_view path, uint32_t mipLevels, bool isDataTexture) override;
	virtual uint32_t createTexture() override;
	virtual uint32_t createTexture(TextureConfig textureConfig);
	virtual uint32_t createDepthTexture(uint32_t width, uint32_t height, uint32_t mipLevels) override;
	virtual TextureVulkan* getTexture(uint32_t id) override;
	virtual void* inspectTexture(uint32_t id) override;
	virtual uint32_t getInspectorLayout();

	void registerTextureSampler(uint32_t textureID);
	void registerTextureStorage(uint32_t textureID, VkImageLayout layout);
	uint32_t getBindlessTextureLayout();
	uint32_t getBindlessSet();

private:
	void _loadTexture(std::string_view path, uint32_t mipLevels, bool isDataTexture);

	// the id of the raw texture, returned back to user the inspectable texture
	void _createInspectorDescriptorBind();
	void _createBindlessDescriptor();

private:

	RenderDeviceVulkan* renderDeviceVulkan;
	BufferManagerVulkan* vulkanBufferManager;
	DescriptorManagerVulkan* descriptorManagerVulkan;

	uint32_t inspectorLayoutID;
	uint32_t inspectorPoolID;
	uint32_t inspectorSetID;

	uint32_t globalBindlessLayoutID;
	uint32_t globalBindlessPoolID;
	uint32_t globalBindlessSetID;
};

