#pragma once

#include "PostProcessRendererVulkan.h"

class BlurPassVulkan;

class AmbientOcclusionPassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstantInfo {
		alignas(64) glm::mat4 invProj;
    	alignas(4) 	float radius;
		alignas(4) 	float bias;
		alignas(4) 	float intensity;
		alignas(4) 	float projScale;
	};

	struct GTAOPushConstant {
		glm::mat4 invProj;
		float radius;
		float intensity;
		float projScale;
		int sliceCount;
		int stepCount;
	};

	struct RTAOPushConstant {
		glm::mat4 invViewProj;
		glm::vec4 cameraPos;
		float radius;
		float intensity;
		int rayCount;
	};

	enum class Method : int { Alchemy = 0, GTAO = 1, RayTraced = 2 };

public:
	AmbientOcclusionPassVulkan(std::string serviceName = "AmbientOcclusionPassVulkan");

	virtual ~AmbientOcclusionPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	void writeAO(VkCommandBuffer cmd, uint32_t currentFrame, VulkanPipeline* pipeline, VkDescriptorSet set, const void* push, uint32_t pushSize);

protected:
	AmbientOcclusionSettings* settings { nullptr };
	BlurPassVulkan* blurPass { nullptr };
	void _createOcclusionMap();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptorSetsAO(uint32_t index);
	void _updateDescriptorSetsRT(uint32_t index, const VkWriteDescriptorSetAccelerationStructureKHR& tlasInfo);
	void _recreateResources();
	void _cleanupResources();

	uint32_t aoMapID;
	TextureVulkan* aoMap { nullptr };
	RGHandle aoMapHandle {};
	TextureVulkan* depthImage { nullptr };
	RGHandle depthHandle {};

	uint32_t occlusionDescriptorLayoutID;
	uint32_t occlusionDescriptorPoolID;
	uint32_t occlusionDecriptorsetsID;
	std::unique_ptr<VulkanPipeline> occlusionPipeline;
	PushConstantInfo pushConstant;

	std::unique_ptr<VulkanPipeline> gtaoPipeline;
	GTAOPushConstant gtaoPush {};

	bool rtSupported { false };
	std::vector<VkDescriptorSetLayoutBinding> rtBindings;
	uint32_t rtLayoutID { 0 };
	uint32_t rtPoolID { 0 };
	uint32_t rtSetsID { 0 };
	std::unique_ptr<VulkanPipeline> rtaoPipeline;
	RTAOPushConstant rtaoPush {};
};