#pragma once

#include "VulkanDevice.h"

// TODO this should be the param to create pipeline
struct PipelineConfigInfo {
	VkPipelineViewportStateCreateInfo viewportInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
	VkPipelineRasterizationStateCreateInfo rasterizationInfo;
	VkPipelineMultisampleStateCreateInfo multisampleInfo;
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	VkPipelineColorBlendStateCreateInfo colorBlendInfo;
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
	std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	uint32_t subpass = 0;
};

class VulkanPipeline
{
public:
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

public:
	VulkanPipeline(VulkanDevice& deviceRef);
	~VulkanPipeline();

	static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t numAttachments);
	
	void create();
	void destroy();
	void bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint);

	void createGraphicsPipeline(
		const std::string& vertFilepath,
		const std::string& fragFilepath,
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
		VkRenderPass renderPass,
		size_t pushConstantSize = 0
	);

	void createGraphicsPipeline(
		const std::string& vertFilepath,
		const std::string& fragFilepath,
		const PipelineConfigInfo& configInfo,
		const VkPipelineVertexInputStateCreateInfo& vertexInputInfo,
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, 
		uint32_t pushConstantSize
	);

	void createComputePipeline(
		const std::string& compFilepath,
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
		uint32_t pushConstantSize
	);

	VkShaderModule createShaderModule(const std::vector<char>& code);

private:
	VulkanPipeline(const VulkanPipeline& other) = delete;
	VulkanPipeline& operator=(const VulkanPipeline& other) = delete;
	VulkanPipeline(const VulkanPipeline&& other) = delete;
	VulkanPipeline& operator=(const VulkanPipeline&& other) = delete;


private:
	VulkanDevice& device;


};

