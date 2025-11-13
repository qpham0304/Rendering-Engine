#pragma once

#include <vulkan/vulkan.h>

class VulkanDevice
{
public:

public:
	VulkanDevice();
	~VulkanDevice();

	void init();
	void onClose();

private:
	VkInstance vkInstance;

private:
	VulkanDevice(const VulkanDevice&) = delete;
	VulkanDevice& operator=(const VulkanDevice&) = delete;
	VulkanDevice(VulkanDevice&&) = delete;
	VulkanDevice& operator=(VulkanDevice&&) = delete;


	void selectPhysicalDevice();
	void createLogicalDevice();
	void createSurface();
};

