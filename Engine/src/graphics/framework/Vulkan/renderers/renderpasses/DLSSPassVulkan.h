#pragma once

#include "PostProcessRendererVulkan.h"
#include <volk.h>

struct DLSSSettings;
struct NVSDK_NGX_Handle;
struct NVSDK_NGX_Parameter;

// upscales the hdr scene from the render size to the output size with NVIDIA DLSS super resolution
class DLSSPassVulkan : public PostProcessRendererVulkan
{
public:
	struct MotionPushConstant {
		glm::mat4 skyReproject;
		glm::vec2 jitter;
	};

public:
	DLSSPassVulkan(std::string serviceName = "DLSSPassVulkan");
	virtual ~DLSSPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	bool isActive() const;
	VkExtent2D getRenderExtent(VkExtent2D outputExtent);
	glm::vec2 getJitter(uint64_t frameNumber) const;

	void writeMotion(VkCommandBuffer cmd, uint32_t currentFrame);
	void evaluate(VkCommandBuffer cmd);

protected:
	void _initNGX();
	void _shutdownNGX();
	bool _createFeature(VkCommandBuffer cmd, VkExtent2D renderExtent, VkExtent2D outputExtent);
	void _releaseFeature();
	void _fetchInputImages();
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _recreateResources() override;
	void _cleanupResources() override;

	DLSSSettings* settings { nullptr };
	NVSDK_NGX_Parameter* m_params { nullptr };
	NVSDK_NGX_Handle* m_feature { nullptr };
	bool m_ngxReady { false };
	bool m_resetHistory { true };

	VkExtent2D m_featureRender {};
	VkExtent2D m_featureOutput {};
	int m_featureMode { -1 };

	VkExtent2D m_queriedOutput {};
	int m_queriedMode { -1 };
	VkExtent2D m_queriedRender {};

	TextureVulkan* colorImage { nullptr };
	TextureVulkan* depthImage { nullptr };
	TextureVulkan* motionImage { nullptr };
	TextureVulkan* dlssMotionImage { nullptr };
	RGHandle dlssMotionHandle {};
	RGHandle outputHandle {};

	MotionPushConstant motionPush {};
	glm::mat4 m_prevSkyViewProj { 1.0f };
};
