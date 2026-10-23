#pragma once

#include "PostProcessRendererVulkan.h"
#include <volk.h>

class DDGIBuilderVulkan;
class ImageBasedRendererVulkan;

class DeferredCombinePassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstant {
        alignas(8) glm::vec2 screenRes;
		alignas(4) float time;
		alignas(4) float frameSeed;
		alignas(4) int shouldCombine;
		alignas(4) float gridSpacing;
		alignas(4) uint32_t probesPerDimension;
		alignas(16) glm::vec4 gridOrigin;
		alignas(4) uint32_t probesResolution;
		alignas(4) float normalBias;
		alignas(4) float ddgiIntensity;
		alignas(4) int ddgiMode;	// 0: off, 1: on, 2: show the ddgi term only
		alignas(16) glm::ivec4 probeCounts;
		alignas(4) float skyIntensity;
	};

	struct UniformBufferObject {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 invProj;
		glm::mat4 invView;
	};

public:
	DeferredCombinePassVulkan(std::string serviceName = "DeferredCombinePassVulkan");
	virtual ~DeferredCombinePassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

    void writeCombinedImage(VkCommandBuffer cmd, uint32_t currentFrame);

private: // TODO: consider make private
	TextureVulkan* denoisedGIImage;
	TextureVulkan* sceneImage;
	TextureVulkan* albedoImage;
	TextureVulkan* positionImage;
	TextureVulkan* normalImage;
	TextureVulkan* pbrImage;
	TextureVulkan* bloomImage;
	TextureVulkan* atlasImageGI;
	TextureVulkan* visibilityAtlasImageGI;
	RGHandle outputHandle {};

protected:
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _recreateResources();
	void _cleanupResources();
	void _fetchInputImages(uint32_t currentFrame);

	UniformBufferObject ubo;
	PushConstant pushConstant;
	std::vector<UniformBufferVulkan*> uniformbuffersList;
	DDGIBuilderVulkan* ddgiRenderer { nullptr };
	ImageBasedRendererVulkan* imageBasedRenderer { nullptr };

};
