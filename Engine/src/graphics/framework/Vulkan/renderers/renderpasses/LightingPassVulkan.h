#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

#include <glm/glm.hpp>
#include <vector>

class GeometryPassVulkan;
class ShadowMapPassVulkan;
class ImageBasedRendererVulkan;
class AmbientOcclusionPassVulkan;

// full screen deferred lighting, samples the g-buffer and writes the hdr scene color with dynamic rendering
class LightingPassVulkan : public RendererVulkan
{
public:
	struct PushConstantLight {
		alignas(64) glm::mat4 sunlightMVP;
		alignas(16) glm::vec4 direction;
		alignas(16) glm::vec4 color;
		alignas(4)  float bias;
		alignas(4)  float alpha;
		alignas(4)  float lintstepLow;
		alignas(4)  float linstepHigh;
		alignas(4)  float litBias;
		alignas(4)  float time;
		alignas(4)	float numLights;
		alignas(4)	float skyboxDetail;
		alignas(4)	int aoOn;
		alignas(4)	float G;
		alignas(4)	float scatteringScale;
		alignas(4)	float iblDiffuse;
	};

	struct LightSSBO {
		alignas(16) glm::vec4 color;
		alignas(16) glm::vec4 position;
		alignas(4) float intensity;
	};

	struct RenderTarget {
		uint32_t width { 0 };
		uint32_t height { 0 };
		TextureVulkan* colorTexture { nullptr };
		RGHandle colorHandle {};
	};

public:
	LightingPassVulkan(std::string serviceName = "LightingPassVulkan");
	virtual ~LightingPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	//TODO: make private once done testing
	RenderTarget renderTarget;
	PushConstantLight pushConstantLight;
	std::vector<LightSSBO> lights;
	float sunIntensity { 10.0f };
	glm::vec4 sunColor { 1.0 };

protected:
	const int numLights = 1000;
	std::vector<StorageBufferVulkan*> lightStoragebuffers;

	std::unique_ptr<VulkanPipeline> lightingPipeline;
	uint32_t lightLayoutID;
	uint32_t lightLayoutID_1;
	uint32_t lightSetsID;
	uint32_t lightSetsID_1;

	GeometryPassVulkan* geometryPass { nullptr };
	ShadowMapPassVulkan* shadowMapPass { nullptr };
	ImageBasedRendererVulkan* imageBasedRenderer { nullptr };
	AmbientOcclusionPassVulkan* ambientOcclusionPass { nullptr };
	TextureVulkan* environmentImage { nullptr };
	TextureVulkan* aoImage { nullptr };

	void _updateLights(uint32_t currentFrame);
	void _renderLightPass(VkCommandBuffer cmd, uint32_t currentFrame);
	void _createRenderTarget();
	void _createLightDescriptor();
	void _updateLightDescriptor();
	void _createLightPipeline();
	void _recreateResources();
	void _cleanupResources();
};
