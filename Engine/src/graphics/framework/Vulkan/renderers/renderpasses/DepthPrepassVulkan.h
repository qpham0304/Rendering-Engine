#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include "graphics/framework/vulkan/core/CommonStructs.h"

#include <glm/glm.hpp>
#include <vector>

// first pass of the deferred path: writes the scene depth only, so depth only passes (ao) do not depend on the g-buffer
// and the g-buffer pass only shades the visible surface (it loads this depth and tests with writes off)
class DepthPrepassVulkan : public RendererVulkan
{
public:
	struct RenderTarget {
		uint32_t width { 0 };
		uint32_t height { 0 };
		VkFormat depthFormat { VK_FORMAT_UNDEFINED };
		TextureVulkan* depthTexture { nullptr };
		RGHandle depthHandle {};
	};

public:
	DepthPrepassVulkan(std::string serviceName = "DepthPrepassVulkan");
	virtual ~DepthPrepassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	//TODO: make private once done testing
	RenderTarget renderTarget;

protected:
	Common::PushBDA pushConstant;

	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;
	std::unique_ptr<VulkanPipeline> depthPipeline;

	void _renderDepth(VkCommandBuffer cmd, uint32_t currentFrame);
	void _createRenderTarget();
	void _createDescriptor();
	void _updateDescriptor();
	void _createPipeline();
	void _recreateResources();
	void _cleanupResources();
};
