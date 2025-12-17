#pragma once

#include "core/resources/managers/TextureManager.h"
#include <vulkan/vulkan.h>
#include "TextureVulkan.h"

class RenderDeviceVulkan;
class BufferManagerVulkan;

class TextureManagerVulkan : public TextureManager
{
public:
	TextureManagerVulkan(std::string serviceName = "TextureManagerVulkan");	
	~TextureManagerVulkan();

	virtual int init(WindowConfig config) override;
	virtual int onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual uint32_t loadTexture(std::string_view path) override;
	virtual uint32_t createDepthTexture() override;
	virtual TextureVulkan* getTexture(uint32_t id) override;

	void createImage(
		uint32_t width, 
		uint32_t height, 
		VkFormat format, 
		VkImageTiling tiling, 
		VkImageUsageFlags usage, 
		VkMemoryPropertyFlags properties,
		VkImage& image, 
		VkDeviceMemory& imageMemory
	);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void createTextureSampler(VkSampler& textureSampler);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

protected:
	VkFormat findDepthFormat();
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);



private:
	RenderDeviceVulkan* renderDeviceVulkan;
	BufferManagerVulkan* vulkanBufferManager;
};

