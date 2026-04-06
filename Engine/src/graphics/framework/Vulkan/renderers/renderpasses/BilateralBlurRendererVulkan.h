#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

class BilateralBlurRendererVulkan : public RendererVulkan
{
public:
	struct PushConstantInfo {
		alignas(64) glm::mat4 view;
    	alignas(4) 	float radius;
		alignas(4) 	float bias;
		alignas(4) 	float intensity;
	};

public:
	BilateralBlurRendererVulkan(std::string serviceName = "BilateralBlurRendererVulkan");

	virtual ~BilateralBlurRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

	void writeBlur();

private:
	void _createOcclusionMap();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptorSets(uint32_t index);
	void _recreateResources();
	void _cleanupResources();

	uint32_t aoMapID;
	TextureVulkan* aoMap;
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
	uint32_t blurDescriptorSetsID;
	std::unique_ptr<VulkanPipeline> blurPipeline;
};