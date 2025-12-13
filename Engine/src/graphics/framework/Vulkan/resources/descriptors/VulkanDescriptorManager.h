#pragma once

#include "src/graphics/framework/Vulkan/core/WrapperStructs.h"
#include "src/core/resources/managers/Manager.h"
#include <vulkan/vulkan.h>

class RenderDeviceVulkan;

class VulkanDescriptorManager : public Manager
{
public:
	VulkanDescriptorManager();
	~VulkanDescriptorManager();

	virtual int init();
	virtual int onClose();
	virtual void destroy(uint32_t id);

	uint32_t createLayout(std::vector<VkDescriptorSetLayoutBinding> bindings);
	uint32_t createPool(std::vector<VkDescriptorPoolSize> poolSizes, uint32_t maxSets);
	uint32_t createSet();
	void write();
	void bindDescriptorSets();

private:
	RenderDeviceVulkan* renderDeviceVulkan;

	std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;
	std::unordered_map<uint32_t, VkDescriptorPool> descriptorPools;
	std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets;
};

