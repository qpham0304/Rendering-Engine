#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

class PostProcessRendererVulkan : public RendererVulkan
{
public:
	struct PushConstant {
		float isVertical;
		int blurRadius;
		float scale;
	};

public:
	PostProcessRendererVulkan(std::string serviceName = "PostProcessRendererVulkan");

	virtual ~PostProcessRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
    virtual TextureVulkan* getOutputImage();

	void createResources();
	void createPipelines();
	void createDescriptors();
	void updateDescriptor(uint32_t index);
	void recreateResources();
	void cleanupResources();
	
protected:
	uint32_t outputImageID;
	TextureVulkan* outputImage;

    uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;
	std::unique_ptr<VulkanPipeline> pipeline;
	PushConstant pushConstant;
};