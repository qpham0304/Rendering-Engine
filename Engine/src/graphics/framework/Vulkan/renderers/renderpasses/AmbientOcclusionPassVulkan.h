#pragma once

#include "PostProcessRendererVulkan.h"

class BlurPassVulkan;

class AmbientOcclusionPassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstantInfo {
		alignas(64) glm::mat4 invProj;
    	alignas(4) 	float radius;
		alignas(4) 	float bias;
		alignas(4) 	float intensity;
		alignas(4) 	float projScale;
	};

public:
	AmbientOcclusionPassVulkan(std::string serviceName = "AmbientOcclusionPassVulkan");

	virtual ~AmbientOcclusionPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	void writeAO(VkCommandBuffer cmd, uint32_t currentFrame);

protected:
	AmbientOcclusionSettings* settings { nullptr };
	BlurPassVulkan* blurPass { nullptr };
	void _createOcclusionMap();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptorSetsAO(uint32_t index);
	void _recreateResources();
	void _cleanupResources();

	uint32_t aoMapID;
	TextureVulkan* aoMap { nullptr };
	RGHandle aoMapHandle {};
	TextureVulkan* depthImage { nullptr };
	RGHandle depthHandle {};

	uint32_t occlusionDescriptorLayoutID;
	uint32_t occlusionDescriptorPoolID;
	uint32_t occlusionDecriptorsetsID;
	std::unique_ptr<VulkanPipeline> occlusionPipeline;
	PushConstantInfo pushConstant;
};