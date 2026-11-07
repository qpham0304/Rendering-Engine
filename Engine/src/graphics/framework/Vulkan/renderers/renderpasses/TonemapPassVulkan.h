#pragma once

#include "PostProcessRendererVulkan.h"
#include <volk.h>

struct TonemapSettings;

// hdr scene (upscaled by dlss when it runs) to the display image at the output size
class TonemapPassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstant {
		int tonemapper;
		float exposure;
		float invGamma;
	};

public:
	TonemapPassVulkan(std::string serviceName = "TonemapPassVulkan");
	virtual ~TonemapPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	void writeTonemap(VkCommandBuffer cmd, uint32_t currentFrame);

protected:
	void _fetchInputImage();
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _recreateResources() override;
	void _cleanupResources() override;

	TonemapSettings* settings { nullptr };
	PushConstant tonemapPush {};
	TextureVulkan* inputImage { nullptr };
	RGHandle outputHandle {};
};
