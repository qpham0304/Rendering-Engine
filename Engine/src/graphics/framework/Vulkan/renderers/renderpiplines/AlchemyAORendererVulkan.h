#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

class AlchemyAORendererVulkan : public RendererVulkan
{
public:
	struct PushConstantInfo {
		alignas(64) glm::mat4 view;
    	alignas(4) 	float radius;
		alignas(4) 	float bias;
		alignas(4) 	float intensity;
		alignas(4) 	float projScale;
	};

	struct BlurPushConstantInfo {
		float isVertical;
		int blurRadius;
		float scale;
	};

public:
	AlchemyAORendererVulkan(std::string serviceName = "AlchemyAORendererVulkan");

	virtual ~AlchemyAORendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

	void writeAO(VkCommandBuffer cmd, uint32_t currentFrame);
	void writeBlur(VkCommandBuffer cmd, uint32_t currentFrame);

public:	//TODO: make this private after test done
	void _createOcclusionMap();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptorSetsAO(uint32_t index);
	void _updateDescriptorSetsBlur(uint32_t index);
	void _recreateResources();
	void _cleanupResources();

	uint32_t aoMapID;
	uint32_t aoMapTempID;
	TextureVulkan* aoMap;
	TextureVulkan* aoMapTemp;
	TextureVulkan* depthImage;
	TextureVulkan* positionImage;
	TextureVulkan* normalImage;

	uint32_t occlusionDescriptorLayoutID;
	uint32_t occlusionDescriptorPoolID;
	uint32_t occlusionDecriptorsetsID;
	std::unique_ptr<VulkanPipeline> occlusionPipeline;
	PushConstantInfo pushConstant;

	uint32_t blurDescriptorLayoutID;
	uint32_t blurDescriptorPoolID;
	uint32_t blurDescSetMtoT_ID;
	uint32_t blurDescSetTtoM_ID;
	std::unique_ptr<VulkanPipeline> blurPipeline;
	BlurPushConstantInfo blurrPushConstant;
};