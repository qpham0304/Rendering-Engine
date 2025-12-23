#include "VulkanUtils.h"
#include "VulkanSwapChain.h"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <iostream>

uint32_t VulkanUtils::findMemoryType(
        VkPhysicalDevice physicalDevice, 
        uint32_t typeFilter, 
        VkMemoryPropertyFlags properties
) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

std::vector<char> VulkanUtils::readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Device::readFile: failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

uint32_t VulkanUtils::numFrames()
{
    return VulkanSwapChain::MAX_FRAMES_IN_FLIGHT;
}
