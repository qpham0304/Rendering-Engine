#include "TextureVulkan.h"
#include "vulkan/vulkan.h"
#include <stdexcept>

TextureVulkan::TextureVulkan(uint32_t id) : Texture(id)
{

}

TextureVulkan::~TextureVulkan()
{

}

void TextureVulkan::Bind()
{

}

void TextureVulkan::Unbind()
{

}

void TextureVulkan::Delete()
{

}

void TextureVulkan::destroy(VkDevice device)
{
	vkDestroySampler(device, textureSampler, nullptr);
	vkDestroyImageView(device, textureImageView, nullptr);
	vkDestroyImage(device, textureImage, nullptr);
	vkFreeMemory(device, textureImageMemory, nullptr);
}

