#include "DescriptorVulkan.h"
#include <core/features/ServiceLocator.h>
#include <graphics/framework/Vulkan/resources/buffers/BufferManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/BufferVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <core/events/EventManager.h>

DescriptorVulkan::DescriptorVulkan(uint32_t layoutID, uint32_t poolID, uint32_t setID)
    : m_descriptorLayoutID(layoutID), m_descriptorPoolID(poolID), m_descriptorSetID(setID)
{
	setFootprint("DescriptorVulkan", sizeof(DescriptorVulkan), MemorySystem::Graphics);
	DescriptorManager& descriptorManagerTemp = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	m_descriptorManager = &dynamic_cast<DescriptorManagerVulkan&>(descriptorManagerTemp);
	
}

DescriptorVulkan::DescriptorVulkan(const DescriptorVulkan &other)
{
    m_descriptorLayoutID = other.m_descriptorLayoutID;
    m_descriptorPoolID = other.m_descriptorPoolID;
    m_descriptorSetID = other.m_descriptorSetID;
    m_descriptorManager = other.m_descriptorManager;
}

DescriptorVulkan& DescriptorVulkan::operator=(const DescriptorVulkan &other)
{
    if(this != &other) {
        m_descriptorLayoutID = other.m_descriptorLayoutID;
        m_descriptorPoolID = other.m_descriptorPoolID;
        m_descriptorSetID = other.m_descriptorSetID;
        m_descriptorManager = other.m_descriptorManager;
    }
    return *this;
}

VkDescriptorSetLayout DescriptorVulkan::layout() const
{
    return m_descriptorManager->getDescriptorLayout(m_descriptorLayoutID);
}

VkDescriptorPool DescriptorVulkan::pool() const
{
    return m_descriptorManager->getDescriptorPool(m_descriptorPoolID);
}

const std::vector<VkDescriptorSet>& DescriptorVulkan::sets() const
{
    return m_descriptorManager->getDescriptorSet(m_descriptorSetID);
}
