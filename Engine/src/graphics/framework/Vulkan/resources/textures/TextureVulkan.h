#pragma once

#include "core/features/Texture.h"
#include "vulkan/vulkan.h"

class BufferVulkan;
class BufferManagerVulkan;
class DescriptorManagerVulkan;
class RenderDeviceVulkan;
class TextureManagerVulkan;

struct TextureConfig {	
	uint32_t width = 0;
	uint32_t height = 0; 
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; 
	VkImageAspectFlagBits aspectBits = VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	VkMemoryPropertyFlagBits properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	uint32_t mipLevels = 1;
	uint32_t layerCount = 1;
};

struct TextureSamplerConfig {
	VkFilter filter = VK_FILTER_LINEAR;
	VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
};

class TextureVulkan : public Texture
{
public:
	friend class TextureManagerVulkan;
	
	TextureVulkan();
	TextureVulkan(uint32_t id);
	virtual ~TextureVulkan() override;

	void init(TextureConfig config, TextureSamplerConfig sampler);
	uint32_t getMipLevels();
	uint32_t getLayerCount();
	void transitImage(VkCommandBuffer cmd, VkImageLayout curLayout, VkImageLayout newLayout, int mip, int layer);
	void transitImage(VkCommandBuffer cmd, VkImageLayout curLayout, VkImageLayout newLayout);
	void copyFrom(VkCommandBuffer cmd, TextureVulkan* src);

protected:
	virtual void Bind() override {};
	virtual void Unbind() override {};
	virtual void Delete() override {};
	virtual void loadTexture(const char* path, bool flip) {};

public:
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	const VkDescriptorImageInfo& getDescImageInfoGeneral();
	const VkDescriptorImageInfo& getDescImageInfoReadOnly();
	const TextureSamplerConfig& getSamplerInfo();

private:
	void destroy(VkDevice device);

	VkImageLayout currentLayout;
	VkFormat format;
	VkImageUsageFlags usage;
	VkMemoryPropertyFlags properties;
	VkImageAspectFlagBits aspectBits;
	VkImageTiling tiling;
	
	uint32_t mipLevels = 1;
	uint32_t layerCount = 1;
	TextureSamplerConfig samplerInfo;

    VkDescriptorImageInfo imageInfoGeneral{};
    VkDescriptorImageInfo imageInfoReadOnly{};

	std::vector<BufferVulkan*> buffers;
	BufferManagerVulkan* bufferManager{ nullptr };
	DescriptorManagerVulkan* descriptorManager{ nullptr };
	TextureManagerVulkan* textureManager{ nullptr };
	RenderDeviceVulkan* renderDevice{ nullptr };
};

