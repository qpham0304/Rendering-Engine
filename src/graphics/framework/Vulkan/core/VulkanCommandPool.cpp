#include "VulkanCommandPool.h"
#include "VulkanSwapChain.h"
#include "../../src/logging/Logger.h"
#include "../../src/core/features/ServiceLocator.h"
#include "../RenderDeviceVulkan.h"

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


void VulkanCommandPool::beginBuffer(VkCommandBufferUsageFlags usageFlags)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;					// Optional
	beginInfo.pInheritanceInfo = nullptr;	// Optional

	if (vkBeginCommandBuffer(currentBuffer(), &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

}

void VulkanCommandPool::endBuffer()
{
	if (vkEndCommandBuffer(currentBuffer()) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void VulkanCommandPool::draw(uint32_t verticesCount)
{
	vkCmdDraw(currentBuffer(), static_cast<uint32_t>(verticesCount), 1, 0, 0);
}

void VulkanCommandPool::drawIndexed(uint32_t indexCount)
{
	vkCmdDrawIndexed(currentBuffer(), static_cast<uint32_t>(indexCount), 1, 0, 0, 0);
}

void VulkanCommandPool::drawInstanced(uint32_t indexCount, uint32_t instanceCount)
{
	Logger& logger = ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
	logger.warn("Draw Instance is not supported yet");
}

VkCommandBuffer VulkanCommandPool::currentBuffer()
{
	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	RenderDeviceVulkan& renderDeviceVulkan = static_cast<RenderDeviceVulkan&>(device);

	return commandBuffers[renderDeviceVulkan.getCurrentFrame()];
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

VkCommandBuffer VulkanCommandPool::beginSingleTimeCommand()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VulkanCommandPool::endSingleTimeCommand(VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device.graphicsQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}