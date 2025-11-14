#include "VulkanBuffer.h"
#include "stdexcept"
#include "../RenderDeviceVulkan.h"
#include "VulkanSwapChain.h"

VulkanBuffer::VulkanBuffer(VulkanDevice& deviceRef)
	: device(deviceRef)
{
}

VulkanBuffer::~VulkanBuffer()
{
	throw std::runtime_error("VulkanBuffer ~VulkanBuffer(): Unimplemented error");

}

void VulkanBuffer::create()
{
	throw std::runtime_error("VulkanBuffer create(): Unimplemented error");
}

uint32_t VulkanBuffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(device.physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanBuffer::destroy()
{
	throw std::runtime_error("VulkanBuffer destroy(): Unimplemented error");

}

void VulkanBuffer::createBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkBuffer& buffer,
	VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device.device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(device.device, buffer, bufferMemory, 0);
}

void VulkanBuffer::createVertexBuffer(std::vector<VulkanDevice::Vertex> vertices, VkCommandPool commandPool)
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vkMapMemory(device.device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(device.device, stagingBufferMemory);


	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer,
		vertexBufferMemory
	);

	copyBuffer(stagingBuffer, vertexBuffer, bufferSize, commandPool);

	vkDestroyBuffer(device.device, stagingBuffer, nullptr);
	vkFreeMemory(device.device, stagingBufferMemory, nullptr);
}


void VulkanBuffer::createIndexBuffer(std::vector<uint16_t> indices, VkCommandPool commandPool)
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vkMapMemory(device.device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(device.device, stagingBufferMemory);

	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer,
		indexBufferMemory
	);

	copyBuffer(stagingBuffer, indexBuffer, bufferSize, commandPool);

	vkDestroyBuffer(device.device, stagingBuffer, nullptr);
	vkFreeMemory(device.device, stagingBufferMemory, nullptr);
}


void VulkanBuffer::createCombinedBuffer(std::vector<VulkanDevice::Vertex> vertices, std::vector<uint16_t> indices, VkCommandPool commandPool)
{
	VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
	VkDeviceSize wholeBufferSize = vertexBufferSize + indexBufferSize;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(
		wholeBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vkMapMemory(device.device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);
	memcpy(data, vertices.data(), (size_t)vertexBufferSize);
	memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), (size_t)vertexBufferSize);
	vkUnmapMemory(device.device, stagingBufferMemory);


	createBuffer(
		wholeBufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		combinedBuffer,
		combinedBufferMemory
	);

	copyBuffer(stagingBuffer, combinedBuffer, wholeBufferSize, commandPool);

	vkDestroyBuffer(device.device, stagingBuffer, nullptr);
	vkFreeMemory(device.device, stagingBufferMemory, nullptr);
}

void VulkanBuffer::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(VulkanDevice::UniformBufferObject);

	uniformBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniformBuffers[i],
			uniformBuffersMemory[i]
		);

		vkMapMemory(device.device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}
}

void VulkanBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandPool commandPool)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device.device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device.graphicsQueue);

	vkFreeCommandBuffers(device.device, commandPool, 1, &commandBuffer);
}
