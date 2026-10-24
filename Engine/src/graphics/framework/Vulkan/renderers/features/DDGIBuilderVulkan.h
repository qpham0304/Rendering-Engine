#pragma once

#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/RendererVulkan.h"


class RayTraceRendererVulkan;
class ImageBasedRendererVulkan;

// Dynamic diffuse global illumination, one probe grid (LightProbeComponent) per scene.
// Every frame:
//   writeTrace             every probe shoots raysPerProbe rays, radiance + hit distance land in the ray buffers
//   writeUpdateIrradiance  ray buffers are blended into the irradiance atlas
//   writeUpdateVisibility  ray buffers are blended into the visibility (distance) atlas
// The passes write the current atlases and copy them to the lastFrame ones,
// everything that samples the probes (the trace pass itself and DeferredCombinePassVulkan) reads lastFrame.
// The grid indexing and the atlas layout are documented in assets/shaders/common/ddgi.glsl
class DDGIBuilderVulkan : public RendererVulkan
{
private:
	struct PushConstant {
		uint64_t objectsRef;
		uint64_t lightsRef;
		glm::vec4 gridOrigin;    // vec4 for alignment but this can be vec3
		glm::ivec4 probeCounts;  // xyz: probes per axis, w: total
		float gridSpacing;
		uint32_t probesResolution;
		uint32_t lightCount;
	};

	struct UniformBufferObject {
		glm::mat4 randomRotation;	// rotates the ray directions, the update passes need the same one the trace pass used
		glm::vec4 sunDirection;		// xyz: toward the sun
		glm::vec4 sunColor;			// rgb: color * intensity
		float hysteresis;			// how much of the previous frame the atlases keep
		float maxRayDistance;
		float normalBias;
		uint32_t clear;				// 1 = ignore the history
		float skyIntensity;			// scales the environment light the probes receive
		float padding[3];
	};

public:
    DDGIBuilderVulkan();
    virtual ~DDGIBuilderVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	void writeTrace(VkCommandBuffer cmd, uint32_t currentFrame);
	void writeUpdateVisibility(VkCommandBuffer cmd, uint32_t currentFrame);
	void writeUpdateIrradiance(VkCommandBuffer cmd, uint32_t currentFrame);
	TextureVulkan* getAtlasImage();
	TextureVulkan* getVisibilityAtlasImage();

private:
	TextureVulkan* rayColorBuffer { nullptr };
	TextureVulkan* rayDistanceBuffer { nullptr };
	TextureVulkan* currentIrradianceAtlas { nullptr };
	TextureVulkan* lastframeIrradianceAtlas { nullptr };
	TextureVulkan* currentVisibilityAtlas { nullptr };
	TextureVulkan* lastFrameVisibilityAtlas { nullptr };

	RGHandle rayColorHandle {};
	RGHandle rayDistanceHandle {};
	RGHandle currentIrradianceHandle {};
	RGHandle lastIrradianceHandle {};
	RGHandle currentVisibilityHandle {};
	RGHandle lastVisibilityHandle {};

	uint32_t probTracePipelineLayoutID;
	uint32_t probTracePipelinePoolID;
	uint32_t probTracePipelineSetID;
	std::unique_ptr<VulkanPipeline> probTracePipeline;
    std::vector<VkDescriptorSetLayoutBinding> probTraceBindings;

	uint32_t updateIrradianceLayoutID;
	uint32_t updateIrradiancePoolID;
	uint32_t updateIrradianceSetID;
	std::unique_ptr<VulkanPipeline> updateIrradiancePipeline;

	uint32_t updateVisibilityLayoutID;
	uint32_t updateVisibilityPoolID;
	uint32_t updateVisibilitySetID;
	std::unique_ptr<VulkanPipeline> updateVisibilityPipeline;

	std::vector<UniformBufferVulkan*> uniformbuffersList;
	UniformBufferObject ubo{};
	PushConstant pushConstant{};
    RayTraceRendererVulkan* raytracer { nullptr };
	ImageBasedRendererVulkan* imageBasedRenderer { nullptr };

	// these three must match the constants in assets/shaders/common/ddgi.glsl
    const uint32_t raysPerProbe = 64;
	const uint32_t IRRADIANCE_RES = 8;
	const uint32_t VISIBILITY_RES = 16;
	// the atlas is Cx tiles wide and Cy * Cz tiles tall, this keeps it inside the max image size
	const uint32_t MAX_PROBES_PER_DIMENSION = 24;

	glm::uvec3 probeCounts { 1 };
	uint32_t irrAtlasW { 0 };
	uint32_t irrAtlasH { 0 };
	uint32_t visAtlasW { 0 };
	uint32_t visAtlasH { 0 };
	uint32_t rayBufferW { 0 };
	uint32_t rayBufferH { 0 };
	uint32_t totalProbes { 0 };
	bool hasProbe { false };
	bool resetHistory { true };

	DDGISettings* settings { nullptr };

	bool _createResources();
	void _createPipeline();
	void _createDescriptor();
	void _updateDescriptor(uint32_t index);
	void _recreateResources() override;
	void _cleanupResources() override;

	void _beginWrite(VkCommandBuffer cmd, TextureVulkan* texture);
	void _endWrite(VkCommandBuffer cmd, TextureVulkan* texture);
	void _copyToLastFrame(VkCommandBuffer cmd, TextureVulkan* current, TextureVulkan* lastFrame);
};
