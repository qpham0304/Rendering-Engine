#include "TextureVulkan.h"
#include "vulkan/vulkan.h"
#include <stdexcept>

TextureVulkan::TextureVulkan() :
	textureSampler(VK_NULL_HANDLE),
	textureImageView(VK_NULL_HANDLE),
	textureImage(VK_NULL_HANDLE),
	textureImageMemory(VK_NULL_HANDLE)
{
	
}

TextureVulkan::TextureVulkan(uint32_t id) : Texture(id),
	textureSampler(VK_NULL_HANDLE),
	textureImageView(VK_NULL_HANDLE),
	textureImage(VK_NULL_HANDLE),
	textureImageMemory(VK_NULL_HANDLE)
{

}

TextureVulkan::~TextureVulkan()
{

}

void TextureVulkan::destroy(VkDevice device)
{
	if (textureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, textureSampler, nullptr);
        textureSampler = VK_NULL_HANDLE;
    }
    if (textureImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, textureImageView, nullptr);
        textureImageView = VK_NULL_HANDLE;
    }
    if (textureImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, textureImage, nullptr);
        textureImage = VK_NULL_HANDLE;
    }
    if (textureImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, textureImageMemory, nullptr);
        textureImageMemory = VK_NULL_HANDLE;
    }
}

