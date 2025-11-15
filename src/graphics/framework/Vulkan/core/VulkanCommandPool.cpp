#include "VulkanCommandPool.h"
#include "VulkanSwapChain.h"

VulkanCommandPool::VulkanCommandPool(VulkanDevice& deviceRef)
	: device(deviceRef)
{

}

VulkanCommandPool::~VulkanCommandPool()
{

}


void VulkanCommandPool::create()
{
	createCommandPool();
	createCommandBuffers();
}

void VulkanCommandPool::destroy()
{
	if (!commandBuffers.empty()) {
		vkFreeCommandBuffers(device.device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	}
	if (commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(device.device, commandPool, nullptr);
	}
}
void VulkanCommandPool::beginBuffer(size_t index, VkCommandBufferUsageFlags usageFlags)
{

}

void VulkanCommandPool::endBuffer(size_t index)
{

}

void VulkanCommandPool::createCommandPool() {
	VulkanDevice::QueueFamilyIndices queueFamilyIndices = device.findQueueFamilies(device.physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(device.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}
}

void VulkanCommandPool::createCommandBuffers() {
	commandBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	if (vkAllocateCommandBuffers(device.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}
}