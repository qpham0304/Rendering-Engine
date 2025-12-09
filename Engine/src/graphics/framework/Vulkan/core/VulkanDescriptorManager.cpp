#include "VulkanDescriptorManager.h"

#include "./VulkanDevice.h"
#include "./VulkanSwapChain.h"
#include "src/core/features/ServiceLocator.h"
#include "../RenderDeviceVulkan.h"

VulkanDescriptorManager::VulkanDescriptorManager()
{

}

VulkanDescriptorManager::~VulkanDescriptorManager()
{

}

void VulkanDescriptorManager::init()
{
	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = static_cast<RenderDeviceVulkan*>(&device);
}
void VulkanDescriptorManager::shutdown()
{

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
	//std::array<VkDescriptorPoolSize, 2> poolSizes{};
	//poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	//poolSizes[0].descriptorCount = static_cast<uint32_t>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
	//poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	//poolSizes[1].descriptorCount = static_cast<uint32_t>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

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