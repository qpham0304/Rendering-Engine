#pragma once

#include "BufferVulkan.h"
#include "vulkan/vulkan.h"

class AccelStructureBufferVulkan : public BufferVulkan
{
public:
	friend class BufferManagerVulkan;

	AccelStructureBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory);

	virtual ~AccelStructureBufferVulkan() override;

	virtual void bind(void* commandBuffer) override;

	uint64_t getAddress();

	VkAccelerationStructureKHR getAccelStr();
	const VkWriteDescriptorSetAccelerationStructureKHR& getDescAccelStructInfo();


protected:
    virtual void destroy(VkDevice device) override;


private:
    VkWriteDescriptorSetAccelerationStructureKHR descASInfo{};
    uint64_t deviceAddress = 0;
	VkAccelerationStructureKHR accelStr = VK_NULL_HANDLE;
};

