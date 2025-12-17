#pragma once

#include "graphics/framework/Vulkan/core/WrapperStructs.h"
#include "core/resources/managers/DescriptorManager.h"
#include <vulkan/vulkan.h>

class RenderDeviceVulkan;

class DescriptorManagerVulkan : public DescriptorManager
{
public:
	DescriptorManagerVulkan(std::string serviceName = "DescriptorManagerVulkan");
	~DescriptorManagerVulkan();

	virtual int init(WindowConfig config) override;
	virtual int onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
	virtual std::vector<uint32_t> listLayoutIDs() const override;
	virtual std::vector<uint32_t> listPoolIDs() const override;

	uint32_t createLayout(std::vector<VkDescriptorSetLayoutBinding> bindings);
	uint32_t createPool(std::vector<VkDescriptorPoolSize> poolSizes, uint32_t maxSets);
	uint32_t createSets(uint32_t layoutID, uint32_t poolID, uint32_t setsCount);
	void writeUniform(
		std::vector<VkWriteDescriptorSet>* writes,
		const VkDescriptorSet& dstSet,
		uint32_t binding,
		const VkDescriptorBufferInfo& bufferInfo
	);

	void writeImage(
		std::vector<VkWriteDescriptorSet>* writes,
		const VkDescriptorSet& dstSet,
		uint32_t binding,
		const VkDescriptorImageInfo& imageInfo
	);

	void writeStorage(
		std::vector<VkWriteDescriptorSet>* writes,
		const VkDescriptorSet& dstSet,
		uint32_t binding,
		const VkDescriptorBufferInfo& bufferInfo
	);
	
	void updateDescriptorSets(const std::vector<VkWriteDescriptorSet>* writes);

	const VkDescriptorSetLayout& getDescriptorLayout(uint32_t id) const;
	const VkDescriptorPool& getDescriptorPool(uint32_t id) const;
	const std::vector<VkDescriptorSet>& getDescriptorSet(uint32_t id) const;


private:
	RenderDeviceVulkan* renderDeviceVulkan;

	std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;
	std::unordered_map<uint32_t, VkDescriptorPool> descriptorPools;
	std::unordered_map<uint32_t, std::vector<VkDescriptorSet>> descriptorSets;
};

