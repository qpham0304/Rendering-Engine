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
		throw std::runtime_error(
			std::string("Device::readFile: failed to open file! ") + filename
		);
	}

	size_t fileSize = (size_t)file.tellg();

	// spir-v is a list of 32 bit words, an empty or truncated file crashes inside the driver instead of failing
	if (fileSize == 0 || fileSize % sizeof(uint32_t) != 0) {
		throw std::runtime_error(
			std::string("Device::readFile: shader file is empty or truncated! ") + filename
		);
	}

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
