#include "BufferManagerVulkan.h"
#include <stdexcept>
#include <chrono>

#include "core/features/Mesh.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include "graphics/framework/vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/vulkan/core/VulkanSwapChain.h"
#include "graphics/framework/vulkan/core/VulkanUtils.h"
#include "IndexBufferVulkan.h"
#include "VertexBufferVulkan.h"
#include "UniformBufferVulkan.h"
#include "StorageBufferVulkan.h"
#include "BufferVulkan.h"
#include "DeviceAddressBufferVulkan.h"
#include "AccelStructureBufferVulkan.h"

BufferManagerVulkan::BufferManagerVulkan(std::string serviceName)
	: BufferManager(serviceName)
{
}

BufferManagerVulkan::~BufferManagerVulkan()
{

}

bool BufferManagerVulkan::init(WindowConfig config)
{
	Service::init(config);

	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&device);

	if (!(renderDeviceVulkan && m_logger)) {
		return false;
	}

	return true;
}

uint32_t BufferManagerVulkan::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(renderDeviceVulkan->device.getPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

//TODO: only support for ray tracing at the moment might want more supports for generic types
uint32_t BufferManagerVulkan::createInstanceBuffer(const VkAccelerationStructureInstanceKHR* instances, uint32_t numInstnaces)
{
	VkDeviceSize bufferSize = sizeof(VkAccelerationStructureInstanceKHR) * numInstnaces;


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
	memcpy(data, instances, (size_t)bufferSize);
	vkUnmapMemory(renderDeviceVulkan->device, stagingBufferMemory);

	VkBuffer buffer;
    VkDeviceMemory bufferMemory;
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT 
			| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		buffer,
		bufferMemory
	);

	copyBuffer(stagingBuffer, buffer, bufferSize);

	vkDestroyBuffer(renderDeviceVulkan->device, stagingBuffer, nullptr);
	vkFreeMemory(renderDeviceVulkan->device, stagingBufferMemory, nullptr);

	auto bufferObject = std::make_shared<BufferVulkan>(m_ids, buffer, bufferMemory);
    
	VkBufferDeviceAddressInfo addressInfo {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.pNext = nullptr;
    addressInfo.buffer = buffer;
    bufferObject->deviceAddress = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &addressInfo);
	
	buffers[m_ids] = bufferObject;
	
	return _assignID();
}

bool BufferManagerVulkan::onClose()
{
	//TODO: destroy() resizes buffers during iteration, check if this break
	for (auto& [id, buffer] : buffers) {
		buffer->destroy(renderDeviceVulkan->device);
	}
	buffers.clear();
	return true;
}

void BufferManagerVulkan::destroy(uint32_t id)
{
	if (buffers.find(id) == buffers.end()) {
		throw std::runtime_error("buffer not found");
	}
	buffers[id]->destroy(renderDeviceVulkan->device);
	buffers.erase(id);
}

std::vector<uint32_t> BufferManagerVulkan::listIDs() const
{
	std::vector<uint32_t> list;
	for (const auto& [id, buffer] : buffers) {
		list.emplace_back(id);
	}
	return list;
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
	VkDeviceMemory& bufferMemory
) {
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

	VkMemoryAllocateFlagsInfo flagsInfo{};
	flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = &flagsInfo;
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
		VK_BUFFER_USAGE_TRANSFER_DST_BIT 
			| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer,
		vertexBufferMemory
	);

	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(renderDeviceVulkan->device, stagingBuffer, nullptr);
	vkFreeMemory(renderDeviceVulkan->device, stagingBufferMemory, nullptr);

	buffers[m_ids] = std::make_shared<VertexBufferVulkan>(
		m_ids,
		vertexBuffer,
		vertexBufferMemory
	);

	return _assignID();
}


uint32_t BufferManagerVulkan::createIndexBuffer(const uint32_t* indices, int size)
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * size;
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
		VK_BUFFER_USAGE_TRANSFER_DST_BIT 
			| VK_BUFFER_USAGE_INDEX_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
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

void BufferManagerVulkan::createUniformBuffers(
	std::vector<UniformBufferVulkan*>& uniformBuffers,
	size_t bufferSize
){
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;

	uniformBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		uint32_t id = createUniformBuffer(uniformBuffer, uniformBufferMemory, bufferSize);
		uniformBuffers[i] = static_cast<UniformBufferVulkan*>(getBuffer(id));
	}
}

uint32_t BufferManagerVulkan::createUniformBuffer(
	VkBuffer& buffer, 
	VkDeviceMemory& buffersMemory, 
	size_t bufferSize
){
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

//TODO: allow customizable HOST_VISIBLE OR DEVICE_LOCAL for performance
void BufferManagerVulkan::createStorageBuffers(std::vector<StorageBufferVulkan*>& storageBuffers, size_t bufferSize)
{
	VkBuffer storageBuffer;
	VkDeviceMemory storageBufferMemory;

	storageBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		uint32_t id = createStorageBuffer(storageBuffer, storageBufferMemory, bufferSize);
		storageBuffers[i] = static_cast<StorageBufferVulkan*>(getBuffer(id));
	}
}

//TODO: allow customizable HOST_VISIBLE OR DEVICE_LOCAL for performance
uint32_t BufferManagerVulkan::createStorageBuffer(VkBuffer& buffer, VkDeviceMemory& buffersMemory, size_t bufferSize)
{
	createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		buffer,
		buffersMemory
    );

    buffers[m_ids] = std::make_shared<StorageBufferVulkan>(m_ids, buffer, buffersMemory, bufferSize);
	StorageBufferVulkan* ref = static_cast<StorageBufferVulkan*>(buffers[m_ids].get());

	vkMapMemory(renderDeviceVulkan->device, buffersMemory, 0, bufferSize, 0, &ref->bufferMapped);

	return _assignID();
}

uint32_t BufferManagerVulkan::createBufferDeviceAddress(size_t bufferSize)
{
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;

	createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        buffer, 
        bufferMemory
    );
	
	auto deviceAddressBuffer = std::make_shared<DeviceAddressBufferVulkan>(m_ids, buffer, bufferMemory);

    VkBufferDeviceAddressInfo addressInfo {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.pNext = nullptr;
    addressInfo.buffer = buffer;
    deviceAddressBuffer->bufferAddressRef = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &addressInfo);
	deviceAddressBuffer->deviceAddress = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &addressInfo);

    vkMapMemory(renderDeviceVulkan->device, deviceAddressBuffer->getMemory(), 0, bufferSize, 0, &deviceAddressBuffer->mappedBuffer);
	
	buffers[m_ids] = deviceAddressBuffer;

    return _assignID();
}

uint32_t BufferManagerVulkan::createAccelStructureBuffer(
	uint64_t maxSize, 
	VkAccelerationStructureCreateInfoKHR& accelInfo
){

	VkBuffer buffer;
	VkDeviceMemory bufferMemory;


	//TODO: check if this fail on machine that does not support acceleration structure
	createBuffer(
        maxSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR 
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer, 
        bufferMemory
    );

	auto accelStructureBuffer = std::make_shared<AccelStructureBufferVulkan>(m_ids, buffer, bufferMemory);
    
	VkBufferDeviceAddressInfo addressInfo {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.pNext = nullptr;
    addressInfo.buffer = buffer;
    accelStructureBuffer->deviceAddress = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &addressInfo);
	
	accelInfo.buffer = static_cast<VkBuffer>(*accelStructureBuffer);
    vkCreateAccelerationStructureKHR(renderDeviceVulkan->device, &accelInfo, nullptr, &accelStructureBuffer->accelStr);

	buffers[m_ids] = accelStructureBuffer;

	return _assignID();
}

uint32_t BufferManagerVulkan::createBufferObject(uint64_t maxSize)
{
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;

	createBuffer(
        maxSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer, 
        bufferMemory
    );

	auto bufferObject = std::make_shared<BufferVulkan>(m_ids, buffer, bufferMemory);
    
	VkBufferDeviceAddressInfo addressInfo {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.pNext = nullptr;
    addressInfo.buffer = buffer;
    bufferObject->deviceAddress = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &addressInfo);
	
	buffers[m_ids] = bufferObject;

    return _assignID();
}

uint32_t BufferManagerVulkan::createBuffer2(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	VkBuffer buffer;
	VkDeviceMemory memory;

	createBuffer(size, usage, properties, buffer, memory);
	
	buffers[m_ids] = std::make_shared<BufferVulkan>(m_ids, buffer, memory);
	if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo addressInfo {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addressInfo.pNext = nullptr;
        addressInfo.buffer = buffer;

        buffers[m_ids]->deviceAddress = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &addressInfo);
    }
    else {
        buffers[m_ids]->deviceAddress = 0;
    }
	
    return _assignID();
}

void BufferManagerVulkan::updateBufferDeviceAddress(uint32_t id, const void *src, size_t bufferSize)
{
    BufferVulkan* buffer = buffers.at(id).get();
	auto deviceAddressBuffer = dynamic_cast<DeviceAddressBufferVulkan*>(buffer);
	if(deviceAddressBuffer) {
		deviceAddressBuffer->update(src, bufferSize);
	}
}

void BufferManagerVulkan::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	renderDeviceVulkan->commandPool.endSingleTimeCommand(commandBuffer);
}

void BufferManagerVulkan::copyBuffer(
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size,
    VkDeviceSize srcOffset,
    VkDeviceSize dstOffset
) {
    VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;

    vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);

	// need to synchonize better if the ssbo is large
	// current solution wait and submit queue which stall the gpu
	// pipeline barrier?
    renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);
}