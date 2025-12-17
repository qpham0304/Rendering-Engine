#pragma once

#include "VulkanDevice.h"

// TODO this should be the param to create pipeline
struct PipelineConfigInfo {
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineViewportStateCreateInfo viewportInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
	VkPipelineRasterizationStateCreateInfo rasterizationInfo;
	VkPipelineMultisampleStateCreateInfo multisampleInfo;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlendInfo;
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
	VkPipelineLayout pipelineLayout = nullptr;
	VkRenderPass renderPass = nullptr;
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

	void create();
	void destroy();
	void bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint);

	void createGraphicsPipeline(
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
		VkRenderPass renderPass,
		size_t pushConstantSize = 0
	);

	void createComputePipeline();



	VkShaderModule createShaderModule(const std::vector<char>& code);

private:
	VulkanPipeline(const VulkanPipeline& other) = delete;
	VulkanPipeline& operator=(const VulkanPipeline& other) = delete;
	VulkanPipeline(const VulkanPipeline&& other) = delete;
	VulkanPipeline& operator=(const VulkanPipeline&& other) = delete;

	static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t width, uint32_t height);

private:
	VulkanDevice& device;


};

