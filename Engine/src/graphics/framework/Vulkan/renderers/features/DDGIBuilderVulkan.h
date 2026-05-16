#pragma once
 
#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
 

class RayTraceRendererVulkan;
class ShadowMapPassVulkan;

class DDGIBuilderVulkan : public RendererVulkan 
{
private:
	struct PushConstant {
		uint64_t objectsRef;
		uint64_t probRef;
		glm::vec4 gridOrigin;    // vec4 for alignment but this can be vec3
		uint32_t probesPerDimension;
		uint32_t probesResolution;
		float gridSpacing;
		glm::vec4 lightDir;
	};

	struct PushConstantBlend {
		uint32_t probesPerDimension;
		uint32_t probesResolution;
		uint32_t numRaysPerProbe;
	};
	
	struct UniformBufferObject {
		glm::mat4 view;
		glm::mat4 prevViewProj;
		glm::mat4 proj;
		glm::vec4 cameraPos;
		glm::mat4 invView;
		glm::mat4 invProj;
		float width;
		float height;
		float frameSeed;
		uint32_t frameCount;
		uint32_t clear;
		// glm::vec4 color;
	};

	struct ObjectDesc {
		uint64_t vertexAddress = 0;
		uint64_t indexAddress = 0;
		uint64_t materialsRef = 0;
		uint64_t materialIndicesRef = 0;
	};

public:
    DDGIBuilderVulkan();
    virtual ~DDGIBuilderVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

	void writeTraceProbe(VkCommandBuffer cmd, uint32_t currentFrame);
	void writeBlendProbe(VkCommandBuffer cmd, uint32_t currentFrame);
	TextureVulkan* getAtlasImage();
	TextureVulkan* getVisibilityAtlasImage();

private:
	uint32_t rayColorBufferID;
	uint32_t rayDistanceBufferID;
	uint32_t atlasID;
	uint32_t prevAtlasID;
	uint32_t currentVisibilityAtlasID;
	uint32_t lastFrameVisibilityAtlasID;
	TextureVulkan* rayColorBuffer;
	TextureVulkan* rayDistanceBuffer;
	TextureVulkan* atlasTexture;
	TextureVulkan* prevAtlasTexture;
	TextureVulkan* currentVisibilityAtlas;
	TextureVulkan* lastFrameVisibilityAtlas;
	
	uint32_t probTracePipelineLayoutID;
	uint32_t probTracePipelinePoolID;
	uint32_t probTracePipelineSetID;
	std::unique_ptr<VulkanPipeline> probTracePipeline;
    std::vector<VkDescriptorSetLayoutBinding> postBindings;

	
	uint32_t blendPipelineLayoutID;
	uint32_t blendPipelinePoolID;
	uint32_t blendPipelineSetID;
	std::unique_ptr<VulkanPipeline> blendPipeline;
    std::vector<VkDescriptorSetLayoutBinding> blendBindings;

	std::vector<UniformBufferVulkan*> uniformbuffersList;
	UniformBufferObject ubo{};
	PushConstant pushConstant;
	PushConstantBlend pushConstantBlend;
    RayTraceRendererVulkan* raytracer { nullptr };
	ShadowMapPassVulkan* shadowMapPass { nullptr };
	glm::mat4 lastViewProj;
	bool firstFrame { true };

    const int raysPerProbe = 64;
	const int PROBE_RES = 8;
	const int numLights = 1000;
	const int MAX_INSTANCES = 5000;
	float atlasW { 0.0f };
	float atlasH { 0.0f };
	float rayBufferW { 0.0f };
	float rayBufferH { 0.0f };
	int totalProbes { 0 };
	
	void _createResources();
	void _createPipeline();
	void _createDescriptor();
	void _updateDescriptor(uint32_t index);
	void _recreateResources();
};