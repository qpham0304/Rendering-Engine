#pragma once

#include <vulkan/vulkan.h>

class VulkanDevice
{

public:
	VulkanDevice();
	~VulkanDevice();

	void create();
	void destroy();


private:
	VulkanDevice(const VulkanDevice&) = delete;
	VulkanDevice& operator=(const VulkanDevice&) = delete;
	VulkanDevice(VulkanDevice&&) = delete;
	VulkanDevice& operator=(VulkanDevice&&) = delete;


	void selectPhysicalDevice();
	void createLogicalDevice();
	void createSurface();

public:
	//TODO: for quick setup, these should be hidden once done
	VkSurfaceKHR surface;
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;

private:


};

