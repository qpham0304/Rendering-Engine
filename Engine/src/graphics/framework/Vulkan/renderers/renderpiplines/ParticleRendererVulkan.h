#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

class TextureVulkan;
class ParticleManager;

class ParticleRendererVulkan : public RendererVulkan
{
private:
	struct EmitterUBO{
		int emitMax{ 100 };
		int emitCount{ 0 };
		int areRecycled{ 0 };
		float emitAccumulator{ 0.0f };
		float emitRate{ 1.0f };
		float lifetimeMin{ 1.0f };
		float lifetimeMax{ 1.0f };
		float speedMin{ 1.0f };
		float speedMax{ 1.0f };
		glm::vec3 spawnPosition{ 0.0f, 0.0f, 0.0f };
		glm::vec3 force{ 0.0f, 0.0f, 0.0f };
		int resetPosition{ 0 };
		glm::vec2 uvOffset { 0.0 };
		glm::vec2 uvScale { 1.0 };
		int textureID { 0 };
		int behaviorType { 0 };
	};

public:
	struct ContainerRefs {
		uint64_t lifetimeBufferRef { 0 };
		uint64_t positionsBufferRef { 0 };
		uint64_t scalesBufferRef { 0 };
		uint64_t velocitiesBufferRef { 0 };
		uint64_t colorsBufferRef { 0 };
	};

	struct ParticlePushConstant {
		uint64_t emitterRef;
		uint64_t containersRef;
		uint32_t containerIdx;
		uint32_t particleCount;
		float deltaTime;
	};

	ParticleRendererVulkan(std::string serviceName = "ParticleRendererVulkan");
	virtual ~ParticleRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

protected:
    const uint32_t MAX_CONTAINERS = 100;
    const uint32_t MAX_EMITTERS = 100;
	const int MAX_INSTANCES = 10000;
	const int numInstances = 1;
	
	std::vector<ContainerRefs> containerRefs { {} };	// attribute refs to one container
	uint32_t containersRefID;
	
	std::vector<std::vector<EmitterUBO>> emittersUBO {};	
	uint32_t emittersUBORefID;
	
	virtual void _recreateResources() override;
	virtual void _cleanupResources() override;
	
	void _createResources();
	void _createPipelines();
	void _createDescriptor();
	void _updateDescriptor();

	void _computeParticle(VkCommandBuffer cmd, uint32_t currentFrame, Scene* scene);
	void _renderParticle(VkCommandBuffer cmd, uint32_t currentFrame, Scene* scene);
	void _createBarrier(VkCommandBuffer cmd, uint32_t bufferID);

	std::unique_ptr<VulkanPipeline> pipeline;
	std::unique_ptr<VulkanPipeline> computePipeline;
	ParticlePushConstant pushConstant;
	
	TextureVulkan* outTexture { nullptr };
	// TextureVulkan* depthTexture { nullptr };

	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;
	std::vector<VkDescriptorSet> descriptorSets;

	ParticleManager* particleManager { nullptr };
};