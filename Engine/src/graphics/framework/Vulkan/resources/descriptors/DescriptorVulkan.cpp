#include "DescriptorVulkan.h"
#include <core/features/ServiceLocator.h>
#include <graphics/framework/Vulkan/resources/buffers/BufferManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/BufferVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <core/events/EventManager.h>
#include <vulkan/vulkan.h>


DescriptorVulkan::DescriptorVulkan()
{
	BufferManager& bufferManagerTemp = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	bufferManager = &dynamic_cast<BufferManagerVulkan&>(bufferManagerTemp);
	DescriptorManager& descriptorManagerTemp = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManager = &dynamic_cast<DescriptorManagerVulkan&>(descriptorManagerTemp);
	
}

void DescriptorVulkan::addBuffer(BufferVulkan *buffer)
{
    buffers.push_back(buffer);
}

void DescriptorVulkan::updateDescriptor()
{
    std::vector<VkWriteDescriptorSet>* writes;

    uint32_t i;
    for(auto& buffer : buffers) {
        
        i++;
    }

    descriptorManager->updateDescriptorSets(writes);
}
