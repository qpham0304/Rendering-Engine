#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/core/CommonStructs.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ShadowMapPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/GeometryPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/LightingPassVulkan.h"
#include "graphics/framework/vulkan/renderers/features/ImageBasedVulkan.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

class AmbientOcclusionPassVulkan;
class HiZPassVulkan;
class SSRGIPassVulkan;

// deferred pipeline: the g-buffer is written by GeometryPassVulkan and lit by LightingPassVulkan,
// this keeps the shared toggles and publishes the g-buffer for the debug views
class DeferredRendererVulkan : public RendererVulkan
{
public:
    DeferredRendererVulkan();
    virtual~DeferredRendererVulkan() override;

    virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

protected:
	DeferredSettings* settings { nullptr };

public:	//TODO: make private once done testing

	ShadowMapPassVulkan* shadowMapPass { nullptr };
	ImageBasedRendererVulkan* imageBasedRenderer { nullptr };
	AmbientOcclusionPassVulkan* ambientOcclusionPass { nullptr };
	HiZPassVulkan* hiZPass { nullptr };
	GeometryPassVulkan* geometryPass { nullptr };
	LightingPassVulkan* lightingPass { nullptr };

	void _recreateResources();
	void _cleanupResources();
};
