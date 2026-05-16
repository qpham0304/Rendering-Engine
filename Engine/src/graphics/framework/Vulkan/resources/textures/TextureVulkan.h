#pragma once

#include "core/features/Texture.h"
#include "graphics/framework/vulkan/core/WrapperStructs.h"

class TextureVulkan : public Texture, protected VkWrap
{
public:
	friend class TextureManagerVulkan;
	
	TextureVulkan();
	TextureVulkan(uint32_t id);
	virtual ~TextureVulkan() override;

	virtual void Bind() override {};
	virtual void Unbind() override {};
	virtual void Delete() override {};

protected:
	virtual void loadTexture(const char* path, bool flip) {};

public:
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	VkImageLayout layout;
	VkFormat format;
	
	uint32_t mipLevel;
	uint32_t layerCount;

	VkDescriptorImageInfo info();

private:
	void destroy(VkDevice device);

};

