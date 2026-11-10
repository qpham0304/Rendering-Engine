#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include "graphics/framework/vulkan/core/CommonStructs.h"

#include <glm/glm.hpp>

struct ToonSettings;
class DepthPrepassVulkan;
class LightingPassVulkan;

// redraws the toon objects over the lit image after the light pass, depth tested equal against the prepass.
// they still go through the prepass and the g-buffer so depth, normals and motion stay valid for the later passes
class ToonPassVulkan : public RendererVulkan
{
public:
	struct PushConstant {
		uint64_t objectsRef;
		uint64_t boneMatricesRef;
		glm::vec4 sunDirection;
		glm::vec4 sunColor;
		glm::vec4 shadowTint;
		glm::vec4 params;
	};

public:
	ToonPassVulkan(std::string serviceName = "ToonPassVulkan");
	virtual ~ToonPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

protected:
	ToonSettings* settings { nullptr };
	DepthPrepassVulkan* depthPrepass { nullptr };
	LightingPassVulkan* lightingPass { nullptr };
	PushConstant pushConstant {};

	DescriptorVulkan descriptor;
	std::unique_ptr<VulkanPipeline> toonPipeline;

	void _renderToon(VkCommandBuffer cmd, uint32_t currentFrame);
	void _createDescriptor();
	void _updateDescriptor();
	void _createPipeline();
	void _recreateResources() override;
	void _cleanupResources() override;
};
