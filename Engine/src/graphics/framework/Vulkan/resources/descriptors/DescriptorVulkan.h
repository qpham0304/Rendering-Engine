#pragma once

#include <vector>
#include "core/features/Object.h"
#include <volk.h>

class BufferVulkan;
class BufferManagerVulkan;
class DescriptorManagerVulkan;

class DescriptorVulkan : public Object {
public:
	DescriptorVulkan() = default;
	DescriptorVulkan(uint32_t layoutID, uint32_t poolID, uint32_t setID);
	DescriptorVulkan(const DescriptorVulkan& other);
	DescriptorVulkan& operator=(const DescriptorVulkan& other);

	~DescriptorVulkan() = default;

	VkDescriptorSetLayout layout() const;
	VkDescriptorPool pool() const;
	const std::vector<VkDescriptorSet>& sets() const;

private:
	uint32_t m_descriptorLayoutID { 0 };
	uint32_t m_descriptorPoolID { 0 };
	uint32_t m_descriptorSetID { 0 };

	DescriptorManagerVulkan* m_descriptorManager{ nullptr };
};
