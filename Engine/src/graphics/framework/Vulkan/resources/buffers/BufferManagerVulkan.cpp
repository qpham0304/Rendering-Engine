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

//TODO: only support for ray tracing at the moment might want more supports for generic types
uint32_t BufferManagerVulkan::createInstanceBuffer(const VkAccelerationStructureInstanceKHR* instances, uint32_t numInstnaces)
{
	VkDeviceSize bufferSize = sizeof(VkAccelerationStructureInstanceKHR) * numInstnaces;


	VkBuffer stagingBuffer;
	VmaAllocation stagingBufferMemory;
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vmaMapMemory(renderDeviceVulkan->device.getAllocator(), stagingBufferMemory, &data);
	memcpy(data, instances, (size_t)bufferSize);
	vmaUnmapMemory(renderDeviceVulkan->device.getAllocator(), stagingBufferMemory);

	VkBuffer buffer;
    VmaAllocation bufferMemory;
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

	vmaDestroyBuffer(renderDeviceVulkan->device.getAllocator(), stagingBuffer, stagingBufferMemory);

	auto bufferObject = std::make_shared<BufferVulkan>(m_ids, buffer, bufferMemory);
    
	VkBufferDeviceAddressInfo addressInfo {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.pNext = nullptr;
    addressInfo.buffer = buffer;
    bufferObject->deviceAddress = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &addressInfo);
	
	buffers[m_ids] = bufferObject;
	
	return _assignID();
}

void BufferManagerVulkan::mapMemory(uint32_t bufferID, size_t size)
{
	BufferVulkan* buffer = getBuffer(bufferID);
	{ VmaAllocationInfo mapInfo{}; vmaGetAllocationInfo(renderDeviceVulkan->device.getAllocator(), buffer->getMemory(), &mapInfo); buffer->mappedBuffer = mapInfo.pMappedData; }
}

void BufferManagerVulkan::unmapMemory(uint32_t bufferID)
{
	BufferVulkan* buffer = getBuffer(bufferID);
	(void)buffer; // persistently mapped (MAPPED_BIT): the mapping lives until the buffer is destroyed
}

bool BufferManagerVulkan::onClose()
{
	//TODO: destroy() resizes buffers during iteration, check if this break
	for (auto& [id, buffer] : buffers) {
		buffer->destroy(renderDeviceVulkan->device, renderDeviceVulkan->device.getAllocator());
	}
	buffers.clear();
	return true;
}

void BufferManagerVulkan::destroy(uint32_t id)
{
	if (buffers.find(id) == buffers.end()) {
		throw std::runtime_error("buffer not found");
	}
	buffers[id]->destroy(renderDeviceVulkan->device, renderDeviceVulkan->device.getAllocator());
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
	if(id == 0) {
		return nullptr;
	}
	if (buffers.find(id) == buffers.end()) {
		throw std::runtime_error("get buffer: buffer not found");
	}
	return buffers[id].get();
}

void BufferManagerVulkan::createBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkBuffer& buffer,
	VmaAllocation& bufferMemory
) {
	if (!renderDeviceVulkan->device.raytracingSupport()) {
		usage &= ~(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// VMA handles memory type selection, sub-allocation and, because the allocator is created with
	// BUFFER_DEVICE_ADDRESS_BIT, the device-address allocate flag whenever usage requests it.
	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.requiredFlags = properties;
	if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		// VMA (AUTO mode) refuses vmaMapMemory unless CPU access is declared at creation.
		// Upload-only buffers (staging, uniforms) get sequential-write; anything the CPU may read back gets random access.
		const bool writeOnly = (usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) != 0;
		allocInfo.flags |= writeOnly ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
		                             : VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
		allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT; // pointer is in VmaAllocationInfo::pMappedData; VMA unmaps on destroy
	}

	VkResult result = vmaCreateBuffer(
		renderDeviceVulkan->device.getAllocator(), &bufferInfo, &allocInfo, &buffer, &bufferMemory, nullptr
	);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}
}

uint32_t BufferManagerVulkan::createVertexBuffer(const void* vertices, int size)
{
	VkDeviceSize bufferSize = size;
	VkBuffer vertexBuffer;
	VmaAllocation vertexBufferMemory;


	VkBuffer stagingBuffer;
	VmaAllocation stagingBufferMemory;
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vmaMapMemory(renderDeviceVulkan->device.getAllocator(), stagingBufferMemory, &data);
	memcpy(data, vertices, (size_t)bufferSize);
	vmaUnmapMemory(renderDeviceVulkan->device.getAllocator(), stagingBufferMemory);

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

	vmaDestroyBuffer(renderDeviceVulkan->device.getAllocator(), stagingBuffer, stagingBufferMemory);

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
	VmaAllocation indexBufferMemory;

	VkBuffer stagingBuffer;
	VmaAllocation stagingBufferMemory;
	createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	void* data;
	vmaMapMemory(renderDeviceVulkan->device.getAllocator(), stagingBufferMemory, &data);
	memcpy(data, indices, (size_t)bufferSize);
	vmaUnmapMemory(renderDeviceVulkan->device.getAllocator(), stagingBufferMemory);

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

	vmaDestroyBuffer(renderDeviceVulkan->device.getAllocator(), stagingBuffer, stagingBufferMemory);

	buffers[m_ids] = std::make_shared<IndexBufferVulkan>(m_ids, indexBuffer, indexBufferMemory);

	return _assignID();
}

void BufferManagerVulkan::createUniformBuffers(
	std::vector<UniformBufferVulkan*>& uniformBuffers,
	size_t bufferSize
){
	VkBuffer uniformBuffer;
	VmaAllocation uniformBufferMemory;

	uniformBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		uint32_t id = createUniformBuffer(uniformBuffer, uniformBufferMemory, bufferSize);
		uniformBuffers[i] = static_cast<UniformBufferVulkan*>(getBuffer(id));
	}
}

uint32_t BufferManagerVulkan::createUniformBuffer(
	VkBuffer& buffer, 
	VmaAllocation& buffersMemory, 
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

	{ VmaAllocationInfo mapInfo{}; vmaGetAllocationInfo(renderDeviceVulkan->device.getAllocator(), buffersMemory, &mapInfo); ref->mappedBuffer = mapInfo.pMappedData; }

	return _assignID();
}

//TODO: allow customizable HOST_VISIBLE OR DEVICE_LOCAL for performance
void BufferManagerVulkan::createStorageBuffers(std::vector<StorageBufferVulkan*>& storageBuffers, size_t bufferSize)
{
	VkBuffer storageBuffer;
	VmaAllocation storageBufferMemory;

	storageBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		uint32_t id = createStorageBuffer(storageBuffer, storageBufferMemory, bufferSize);
		storageBuffers[i] = static_cast<StorageBufferVulkan*>(getBuffer(id));
	}
}

//TODO: allow customizable HOST_VISIBLE OR DEVICE_LOCAL for performance
uint32_t BufferManagerVulkan::createStorageBuffer(VkBuffer& buffer, VmaAllocation& buffersMemory, size_t bufferSize)
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

	{ VmaAllocationInfo mapInfo{}; vmaGetAllocationInfo(renderDeviceVulkan->device.getAllocator(), buffersMemory, &mapInfo); ref->mappedBuffer = mapInfo.pMappedData; }

	return _assignID();
}

uint32_t BufferManagerVulkan::createBufferDeviceAddress(size_t bufferSize)
{
	VkBuffer buffer;
	VmaAllocation bufferMemory;

	createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer, 
        bufferMemory
    );
	
	auto deviceAddressBuffer = std::make_shared<DeviceAddressBufferVulkan>(m_ids, buffer, bufferMemory);

    VkBufferDeviceAddressInfo addressInfo {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.pNext = nullptr;
    addressInfo.buffer = buffer;
	deviceAddressBuffer->deviceAddress = vkGetBufferDeviceAddress(renderDeviceVulkan->device, &addressInfo);

    { VmaAllocationInfo mapInfo{}; vmaGetAllocationInfo(renderDeviceVulkan->device.getAllocator(), deviceAddressBuffer->getMemory(), &mapInfo); deviceAddressBuffer->mappedBuffer = mapInfo.pMappedData; }
	
	buffers[m_ids] = deviceAddressBuffer;

    return _assignID();
}

uint32_t BufferManagerVulkan::createAccelStructureBuffer(
	uint64_t maxSize, 
	VkAccelerationStructureCreateInfoKHR& accelInfo
){

	VkBuffer buffer;
	VmaAllocation bufferMemory;


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
	VmaAllocation bufferMemory;

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
	VmaAllocation memory;

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
	if(id == 0) {
		m_logger->error("failed to update buffer: buffer id: {} invalid", id);
		return;
	}
	
    BufferVulkan* buffer = getBuffer(id);
	buffer->update(src, bufferSize);
}

uint64_t BufferManagerVulkan::getVirtualMemoryAddress(uint32_t id)
{
	return getBuffer(id)->getAddress();
}

uint32_t BufferManagerVulkan::findBufferID(uint64_t virtualAddress)
{
	for(auto& [id, buffer] : buffers) {
		if(buffer->getAddress() == virtualAddress) {
			return id;
		}
	}
	return 0;
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