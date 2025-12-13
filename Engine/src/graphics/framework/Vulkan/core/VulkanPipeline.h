#pragma once

#include "VulkanDevice.h"

static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Device::readFile: failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

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
	struct Pipeline {
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
	};

	std::unordered_map<uint32_t, Pipeline> pipelines;


	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

public:
	VulkanPipeline(VulkanDevice& deviceRef);
	~VulkanPipeline();

	void create();
	void destroy();
	void bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint);

	void createGraphicsPipeline(
		VkDescriptorSetLayout descriptorSetLayout,
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

