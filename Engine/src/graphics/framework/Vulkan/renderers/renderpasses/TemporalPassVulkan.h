#pragma once

#include "PostProcessRendererVulkan.h"
#include <vulkan/vulkan.h>

class TemporalPassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstant {
        alignas(8) glm::vec2 screenRes;
        alignas(4) float maxAccumulation;
	};

	struct UniformBufferObject {
		glm::mat4 invViewProj;   
		glm::mat4 priorViewProj;
	};

public:
	TemporalPassVulkan(std::string serviceName = "TemporalPassVulkan");
	virtual ~TemporalPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

    void writeTP(VkCommandBuffer cmd, uint32_t currentFrame);
    void writeTAA(VkCommandBuffer cmd, uint32_t currentFrame);

protected:
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t currentSetsID, uint32_t index, TextureVulkan* writeImg, TextureVulkan* readImage);
	void _recreateResources();
	void _cleanupResources();
	void _copyImageReource(VkCommandBuffer cmd);

	TextureVulkan* ssrgiImage;
	TextureVulkan* ssrgiHistoryImage;
	TextureVulkan* motionImage;
	TextureVulkan* depthImage;
	TextureVulkan* prevDepthImage;
	TextureVulkan* normalImage;
	TextureVulkan* prevNormalImage;
	std::vector<TextureVulkan*> outputImages;

	uint32_t ssrgiHistoryImageID;
	uint32_t prevDepthImageID;
	uint32_t prevNormalImageID;

	uint32_t TemporalLayoutID;
	uint32_t TemporalPoolID;
	uint32_t TemporalSetsID;
	uint32_t TemporalSetsHistoryID;
	PushConstant pushConstant;
	UniformBufferObject ubo;
	std::vector<UniformBufferVulkan*> uniformbuffersList;

    bool flip { false };
};