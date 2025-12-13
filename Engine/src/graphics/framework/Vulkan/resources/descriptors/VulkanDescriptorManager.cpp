#include "VulkanDescriptorManager.h"

#include "../../core/VulkanDevice.h"
#include "../../core/VulkanSwapChain.h"
#include "../../RenderDeviceVulkan.h"
#include "core/features/ServiceLocator.h"

VulkanDescriptorManager::VulkanDescriptorManager()
{

}

VulkanDescriptorManager::~VulkanDescriptorManager()
{

}

int VulkanDescriptorManager::init()
{
	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&device);

	if (!renderDeviceVulkan) {
		return -1;
	}

	return 0;
}

int VulkanDescriptorManager::onClose()
{
	WriteLock lock = _lockWrite();
	descriptorSetLayouts.clear();
	descriptorPools.clear();
	descriptorSets.clear();

	return 0;
}

void VulkanDescriptorManager::destroy(uint32_t)
{

}

uint32_t VulkanDescriptorManager::createLayout(std::vector<VkDescriptorSetLayoutBinding> bindings)
{
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	descriptorSetLayouts[m_ids] = VkDescriptorSetLayout();

	if (vkCreateDescriptorSetLayout(renderDeviceVulkan->device, &layoutInfo, nullptr, &descriptorSetLayouts[m_ids]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	return _assignID();
}

uint32_t VulkanDescriptorManager::createPool(std::vector<VkDescriptorPoolSize> poolSizes, uint32_t maxSets)
{
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = maxSets;

	descriptorPools[m_ids] = VkDescriptorPool();

	if (vkCreateDescriptorPool(renderDeviceVulkan->device, &poolInfo, nullptr, &descriptorPools[m_ids]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}

	return _assignID();
}

uint32_t VulkanDescriptorManager::createSet()
{
	return _assignID();
}

void VulkanDescriptorManager::write()
{

}

void VulkanDescriptorManager::bindDescriptorSets()
{

}
