#pragma once

#include "core/resources/managers/TextureManager.h"
#include <vulkan/vulkan.h>
#include "TextureVulkan.h"

class RenderDeviceVulkan;
class BufferManagerVulkan;
class VulkanDevice;

class TextureManagerVulkan : public TextureManager
{
public:

	static void createImage(
		uint32_t width,
		uint32_t height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage& image,
		VkDeviceMemory& imageMemory,
		const VulkanDevice& device
	);

	static VkImageView createImageView(
		VkImage image,
		VkFormat format,
		VkImageAspectFlags aspectFlags,
		RenderDeviceVulkan* renderDeviceVulkan
	);

	static void createTextureSampler(
		VkSampler& textureSampler,
		RenderDeviceVulkan* renderDeviceVulkan
	);

	static void transitionImageLayout(
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
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

public:
	TextureManagerVulkan(std::string serviceName = "TextureManagerVulkan");	
	~TextureManagerVulkan();

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual uint32_t loadTexture(std::string_view path) override;
	virtual uint32_t createDepthTexture() override;
	virtual TextureVulkan* getTexture(uint32_t id) override;



private:
	RenderDeviceVulkan* renderDeviceVulkan;
	BufferManagerVulkan* vulkanBufferManager;
};

