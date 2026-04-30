#pragma once

#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ShadowMapRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ImageBasedRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

class AlchemyAORendererVulkan;
class HiZPassVulkan;
class SSRGIPassVulkan;
class DeferredRendererVulkan : public RendererVulkan
{

private:
	struct PushConstant {
		uint64_t objectsRef;
		uint32_t objectIdx;
	};

	struct ObjectDesc {
		uint64_t vertexAddress = 0;
		uint64_t indexAddress = 0;
		uint64_t materialsRef = 0;
		uint64_t materialIndicesRef = 0;
	};

	struct UniformBufferObject {
		glm::mat4 invNormal;
		glm::mat4 view;
		glm::mat4 prevViewProj;
		glm::mat4 proj;
		glm::vec4 cameraPos;
		glm::mat4 invView;
		glm::mat4 invProj;
		float width;
		float height;
	};

	struct StorageBufferObject {
		glm::mat4 model;
	};

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
	};

	struct LightSSBO {
		alignas(16) glm::vec4 color;
		alignas(16) glm::vec4 position;
		alignas(4) float intensity;
	};


public:
    DeferredRendererVulkan();
    virtual~DeferredRendererVulkan() override;

    virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	void renderGui();

	void beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer);
	void endRecording(void* cmdBuffer);
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);

public:	//TODO: make private once done testing	
	const int MAX_INSTANCES = 10000;
	const int numInstances = 1;
	const int numLights = 1000;
	bool showGui{ true };
	uint32_t frameCounter = 0;
	
	std::vector<UniformBufferVulkan*> uniformbuffersList;
	std::vector<StorageBufferVulkan*> storagebuffersList;
	std::vector<StorageBufferVulkan*> prevStoragebufferList;
	std::vector<StorageBufferObject> instanceData;
	std::vector<StorageBufferObject> instanceDataPrev;
	std::vector<StorageBufferVulkan*> lightStoragebuffers;
	PushConstant pushConstant;
	std::vector<LightSSBO> lights;
	PushConstantLight pushConstantLight;

	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;

	VulkanRenderTarget renderTarget;
	std::unique_ptr<VulkanPipeline> gPassPipeline;

	std::unique_ptr<VulkanPipeline> lightingPipeline;
	uint32_t lightLayoutID;
	uint32_t lightLayoutID_1;
	uint32_t lightSetsID;
	uint32_t lightSetsID_1;
	UniformBufferObject ubo{};

	float sunIntensity { 10.0f };
	glm::vec4 sunColor { 1.0 };

	Camera* cam;
	glm::mat4 lastViewProj;
	bool firstFrame { true };

	bool denoiserOn { true };
	bool shouldCombine { true };

	ShadowMapRendererVulkan* shadowMapRenderer { nullptr };
	ImageBasedRendererVulkan* imageBasedRenderer { nullptr };
	AlchemyAORendererVulkan* alchemyAORendererVulkan { nullptr };
	HiZPassVulkan* hiZPassRenderer { nullptr };
	SSRGIPassVulkan* SSRGIPassRenderer { nullptr };
	
	std::unique_ptr<VulkanPipeline> tempPipeline { nullptr };

	
	uint32_t objDeviceAddressBufferID;
	uint64_t objDeviceAddress;
	uint64_t materialsAddress;
	std::vector<ObjectDesc> objects;

	void _renderGeometryPass(VkCommandBuffer cmd, uint32_t currentFrame);
	void _renderLightPass(VkCommandBuffer cmd, uint32_t currentFrame);
	void _createRenderPasses();
	void _createFrameBuffers();
	void _createDescriptor();
	void _updateDescriptor();
	void _createPipelines();

	void _createLightPipeline();
	void _createLightDescriptor();
	void _updateLightDescriptor();

	void _recreateResources();
	void _cleanupResources();
};