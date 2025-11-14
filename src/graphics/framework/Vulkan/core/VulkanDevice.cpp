#include "VulkanDevice.h"
#include "../../src/core/features/ServiceLocator.h"
#include "../../src/Logging/Logger.h"

VulkanDevice::VulkanDevice() 
	:	instance(VK_NULL_HANDLE), 
		surface(VK_NULL_HANDLE),
		physicalDevice(VK_NULL_HANDLE),
		device(VK_NULL_HANDLE)
{

}

VulkanDevice::~VulkanDevice()
{

}

void VulkanDevice::create()
{

}

void VulkanDevice::destroy()
{

}
