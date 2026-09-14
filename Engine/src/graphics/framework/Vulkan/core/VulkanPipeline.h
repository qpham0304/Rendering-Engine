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

    // standard alpha blending usually for physical materials
	void setAlphaBlend() {
		for(auto& attachment : colorBlendAttachments) {
			attachment.blendEnable = VK_TRUE;
			attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachment.colorBlendOp = VK_BLEND_OP_ADD;
			attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		}

	};
    
	// additive alpha blending usually for particle effects and no depth test needed if render separately from the world
	void setAdditiveBlend() {
		for(auto& attachment : colorBlendAttachments) {
			attachment.blendEnable = VK_TRUE;
			attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment.colorBlendOp = VK_BLEND_OP_ADD;
			attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		}
	};
};

struct AttachmentsInfo {
	std::vector<VkFormat> colorAttachmentFormats = {};		// how many output from the shader
	VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
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

	void createGraphicsPipelineDynamic(
		const std::string& vertFilepath,
		const std::string& fragFilepath,
		const PipelineConfigInfo& configInfo,
		const AttachmentsInfo& attachmentsInfo,
		const VkPipelineVertexInputStateCreateInfo& vertexInputInfo,
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, 
		uint32_t pushConstantSize
	);


	void createComputePipeline(
		const std::string& compFilepath,
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
		uint32_t pushConstantSize
	);

	void createRayTracePipeline(
		const std::string& raytraceFilePath,
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

