#include "BufferManagerVulkan.h"
#include <stdexcept>
#include <chrono>

#include "core/features/Mesh.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include "graphics/framework/vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/vulkan/core/VulkanSwapChain.h"
#include "IndexBufferVulkan.h"
#include "VertexBufferVulkan.h"
#include "UniformBufferVulkan.h"
#include "BufferVulkan.h"

BufferManagerVulkan::BufferManagerVulkan()
{
}

BufferManagerVulkan::~BufferManagerVulkan()
{

}

int BufferManagerVulkan::init(WindowConfig config)
{
	Service::init(config);

	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&device);
	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");

	if (!(renderDeviceVulkan && m_logger)) {
		return -1;
	}

	return 0;
}

uint32_t BufferManagerVulkan::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(renderDeviceVulkan->device.physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

int BufferManagerVulkan::onClose()
{
	for (auto& [id, buffer] : buffers) {
		buffer->destroy(renderDeviceVulkan->device);
	}
	buffers.clear();
	return 0;
}

void BufferManagerVulkan::destroy(uint32_t id)
{
	if (buffers.find(id) == buffers.end()) {
		throw std::runtime_error("buffer not found");
	}
	buffers[id]->destroy(renderDeviceVulkan->device);
	buffers.erase(id);
}

void BufferManagerVulkan::bind(uint32_t id)
{
	VkCommandBuffer commandBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	if (buffers.find(id) == buffers.end()) {
		throw std::runtime_error("buffer not found");
	}
	buffers[id]->bind(commandBuffer);
}

BufferVulkan* BufferManagerVulkan::getBuffer(uint32_t id)
{
	if (buffers.find(id) == buffers.end()) {
		throw std::runtime_error("buffer not found");
	}
	return buffers[id].get();
}

void BufferManagerVulkan::createBuffer(
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

	if (vkCreateBuffer(renderDeviceVulkan->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(renderDeviceVulkan->device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(renderDeviceVulkan->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(renderDeviceVulkan->device, buffer, bufferMemory, 0);
}

uint32_t BufferManagerVulkan::createVertexBuffer(const Vertex* vertices, int size)
{
	VkDeviceSize bufferSize = sizeof(Vertex) * size;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;


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
	vkMapMemory(renderDeviceVulkan->device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices, (size_t)bufferSize);
	vkUnmapMemory(renderDeviceVulkan->device, stagingBufferMemory);


	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer,
		vertexBufferMemory
	);

	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(renderDeviceVulkan->device, stagingBuffer, nullptr);
	vkFreeMemory(renderDeviceVulkan->device, stagingBufferMemory, nullptr);

	buffers[m_ids] = std::make_shared<VertexBufferVulkan>(m_ids ,vertexBuffer, vertexBufferMemory);

	return _assignID();
}


uint32_t BufferManagerVulkan::createIndexBuffer(const uint16_t* indices, int size)
{
	VkDeviceSize bufferSize = sizeof(uint16_t) * size;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

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
	vkMapMemory(renderDeviceVulkan->device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices, (size_t)bufferSize);
	vkUnmapMemory(renderDeviceVulkan->device, stagingBufferMemory);

	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer,
		indexBufferMemory
	);

	copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(renderDeviceVulkan->device, stagingBuffer, nullptr);
	vkFreeMemory(renderDeviceVulkan->device, stagingBufferMemory, nullptr);

	buffers[m_ids] = std::make_shared<IndexBufferVulkan>(m_ids, indexBuffer, indexBufferMemory);

	return _assignID();
}

void BufferManagerVulkan::createUniformBuffers(std::vector<UniformBufferVulkan*>& uniformBuffers, size_t bufferSize)
{
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;

	uniformBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		uint32_t id = createUniformBuffer(uniformBuffer, uniformBufferMemory, bufferSize);
		uniformBuffers[i] = (UniformBufferVulkan*)getBuffer(id);
	}
}

uint32_t BufferManagerVulkan::createUniformBuffer(VkBuffer& buffer, VkDeviceMemory& buffersMemory, size_t bufferSize)
{
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		buffer,
		buffersMemory
	);

	buffers[m_ids] = std::make_shared<UniformBufferVulkan>(m_ids, buffer, buffersMemory);
	UniformBufferVulkan* ref = static_cast<UniformBufferVulkan*>(buffers[m_ids].get());

	vkMapMemory(renderDeviceVulkan->device, buffersMemory, 0, bufferSize, 0, &ref->uniformBufferMapped);

	return _assignID();
}


void BufferManagerVulkan::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	renderDeviceVulkan->commandPool.endSingleTimeCommand(commandBuffer);
}

const uint32_t& BufferManagerVulkan::_assignID()
{
	return m_ids.fetch_add(1, std::memory_order_relaxed);
}
