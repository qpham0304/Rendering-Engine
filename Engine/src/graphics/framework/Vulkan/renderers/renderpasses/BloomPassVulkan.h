#pragma once

#include "PostProcessRendererVulkan.h"

class BloomPassVulkan : public PostProcessRendererVulkan
{
public:
	struct UpSamplerPushConstant {
		float filterRadius;
	};

	struct DownSamplerPushConstant {
		glm::vec2 srcResolution;
		int mipLevel;
		float threshold;
		float softThreshold;
	};

	struct BloomPushConstant {
		float exposure;
		float bloomStrength;
		int dirtImageIdx;
	};
	

public:
	BloomPassVulkan(std::string serviceName = "BloomPassVulkan");

	virtual ~BloomPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	void renderGui();

	void writeUpSample(VkCommandBuffer cmd, uint32_t currentFrame);
	void writeDownSample(VkCommandBuffer cmd, uint32_t currentFrame);
	void writeBloomProcess(VkCommandBuffer cmd, uint32_t currentFrame);


protected:	
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _updateUpSampleDescriptors(uint32_t index);
	void _updateDownSampleDescriptors(uint32_t index);
	void _updateBloomProcessDesciptors();
	void _recreateResources();
	void _cleanupResources();
	void _transitionMip(VkCommandBuffer cmd, VkImage image, uint32_t mipLevel, VkImageLayout oldLayout, VkImageLayout newLayout);


	std::unique_ptr<VulkanPipeline> upsamplePipeline;
	UpSamplerPushConstant upPushConstant;

	std::unique_ptr<VulkanPipeline> downsamplePipeline;
	DownSamplerPushConstant downPushConstant;

	std::unique_ptr<VulkanPipeline> bloomPipeline;
	BloomPushConstant bloomPushConstant;

	uint32_t mipCount;

	uint32_t mipDescriptorLayoutID;
	uint32_t mipDescriptorPoolID;
	std::vector<glm::vec2> mipSizes;
	std::vector<VkImageView> mipChain;
	std::vector<VkDescriptorSet> downDescriptorSets;
	std::vector<VkDescriptorSet> upDescriptorSets;

	uint32_t lensDirtTextureID;

	uint32_t bloomDescriptorLayoutID;
	uint32_t bloomDescriptorPoolID;
	uint32_t bloomDecriptorsetsID;

	TextureVulkan* inputImage { nullptr };
};