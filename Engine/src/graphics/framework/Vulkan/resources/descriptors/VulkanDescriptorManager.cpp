#include "DescriptorManagerVulkan.h"

#include "../../core/VulkanDevice.h"
#include "../../core/VulkanSwapChain.h"
#include "../../renderers/RenderDeviceVulkan.h"
#include "core/features/ServiceLocator.h"

DescriptorManagerVulkan::DescriptorManagerVulkan(std::string serviceName)
	: DescriptorManager(serviceName)
{

}

DescriptorManagerVulkan::~DescriptorManagerVulkan()
{

}

int DescriptorManagerVulkan::init(WindowConfig config)
{
	Service::init(config);

	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&device);

	if (!renderDeviceVulkan) {
		return -1;
	}

	return 0;
}

int DescriptorManagerVulkan::onClose()
{
	WriteLock lock = _lockWrite();
	for (auto [id, layout] : descriptorSetLayouts) {
		vkDestroyDescriptorSetLayout(renderDeviceVulkan->device, layout, nullptr);
	}
	descriptorSetLayouts.clear();

	for (auto [id, pool] : descriptorPools) {
		vkDestroyDescriptorPool(renderDeviceVulkan->device, pool, nullptr);
	}
	descriptorPools.clear();

	
	descriptorSets.clear();

	return 0;
}

void DescriptorManagerVulkan::destroy(uint32_t)
{

}

void DescriptorManagerVulkan::bind(uint32_t)
{

}

uint32_t DescriptorManagerVulkan::createLayout(std::vector<VkDescriptorSetLayoutBinding> bindings)
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

uint32_t DescriptorManagerVulkan::createPool(std::vector<VkDescriptorPoolSize> poolSizes, uint32_t maxSets)
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

uint32_t DescriptorManagerVulkan::createSets(uint32_t layoutID, uint32_t poolID, uint32_t setsCount)
{
	std::vector<VkDescriptorSetLayout> layouts(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT, getDescriptorLayout(layoutID));

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = getDescriptorPool(poolID);
	allocInfo.descriptorSetCount = setsCount;
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets[m_ids] = std::vector<VkDescriptorSet>(setsCount);

	if (vkAllocateDescriptorSets(renderDeviceVulkan->device, &allocInfo, descriptorSets[m_ids].data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	return _assignID();
}

void DescriptorManagerVulkan::writeUniform(std::vector<VkWriteDescriptorSet>* writes, VkDescriptorSet& dstSet, const VkDescriptorBufferInfo& bufferInfo)
{
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = dstSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.descriptorCount = 1;
	write.pBufferInfo = &bufferInfo;

	writes->push_back(write);
}

void DescriptorManagerVulkan::writeImage(std::vector<VkWriteDescriptorSet>* writes, VkDescriptorSet& dstSet, const VkDescriptorImageInfo& imageInfo)
{
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = dstSet;
	write.dstBinding = 1;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imageInfo;

	writes->push_back(write);
}

void DescriptorManagerVulkan::updateDescriptorSets(const std::vector<VkWriteDescriptorSet>* writes)
{
	uint32_t size = static_cast<uint32_t>(writes->size());
	vkUpdateDescriptorSets(renderDeviceVulkan->device, size, writes->data(), 0, nullptr);
}

const VkDescriptorSetLayout& DescriptorManagerVulkan::getDescriptorLayout(uint32_t id) const
{
	return descriptorSetLayouts.at(id);
}	

const VkDescriptorPool& DescriptorManagerVulkan::getDescriptorPool(uint32_t id) const
{
	return descriptorPools.at(id);
}	

const std::vector<VkDescriptorSet>& DescriptorManagerVulkan::getDescriptorSet(uint32_t id) const
{
	return descriptorSets.at(id);
}