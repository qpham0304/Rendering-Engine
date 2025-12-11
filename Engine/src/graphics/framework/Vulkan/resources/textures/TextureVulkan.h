#pragma once

#include "Texture.h"
#include "src/graphics/framework/vulkan/core/WrapperStructs.h"

class TextureVulkan : public Texture, protected VkWrap
{
public:
	friend class TextureManagerVulkan;
	
	TextureVulkan(uint32_t id);
	virtual ~TextureVulkan() override;

	virtual void Bind() override;
	virtual void Unbind() override;
	virtual void Delete() override;

protected:
	virtual void loadTexture(const char* path, bool flip) {};

public:	//TODO: temporary public only
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

private:
	void destroy(VkDevice device);

};

