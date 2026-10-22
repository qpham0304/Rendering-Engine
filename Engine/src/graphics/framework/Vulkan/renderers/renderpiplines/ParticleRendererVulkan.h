#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include <array>
#include <unordered_map>
#include <unordered_set>

class TextureVulkan;
class ParticleManager;
class GeometryPassVulkan;
class LightingPassVulkan;
struct ParticleEmitter;

class ParticleRendererVulkan : public RendererVulkan
{
private:
	// TODO: currently match shaders/common/particles.glsl defines, make shared struct for this
	struct EmitterGPU {
		glm::mat4 transform { 1.0f };
		glm::vec4 colorStart { 1.0f };
		glm::vec4 colorEnd { 1.0f };
		glm::vec3 shapeExtents { 1.0f };	float shapeRadius { 1.0f };
		glm::vec3 direction { 0.0f, 1.0f, 0.0f };	float spreadAngle { 0.0f };
		glm::vec3 force { 0.0f };			float gravityScale { 0.0f };
		float shapeConeAngle { 0.0f };
		float speedMin { 0.0f };
		float speedMax { 0.0f };
		float lifetimeMin { 1.0f };
		float lifetimeMax { 1.0f };
		float drag { 0.0f };
		float turbulenceStrength { 0.0f };
		float turbulenceFrequency { 1.0f };
		float vortexStrength { 0.0f };
		float attractStrength { 0.0f };
		float sizeMin { 0.1f };
		float sizeMax { 0.1f };
		float sizeEndScale { 1.0f };
		float rotationMin { 0.0f };
		float rotationMax { 0.0f };
		float angularVelocityMin { 0.0f };
		float angularVelocityMax { 0.0f };
		float fadeIn { 0.0f };
		float fadeOut { 0.0f };
		float stretchFactor { 0.0f };
		float flipbookFPS { 0.0f };
		float meshShading { 0.0f };
		int spawnShape { 0 };
		int velocityFromShape { 0 };
		int localSpace { 0 };
		int renderMode { 0 };
		int textureID { 0 };
		int flipbookRows { 1 };
		int flipbookCols { 1 };
		int emitFromShell { 0 };
		uint64_t meshVerticesRef { 0 };
		uint64_t meshIndicesRef { 0 };
	};
	static_assert(sizeof(EmitterGPU) == 280, "EmitterGPU must match Emitter in particles.glsl");

	struct MeshRefs {
		uint64_t verticesRef { 0 };
		uint64_t indicesRef { 0 };
		uint32_t indexCount { 0 };
		uint32_t albedoTextureID { 0 };	// 0 when the model's material only has the fallback albedo
	};

	// one emitter that has work this frame, gathered once and shared by the compute and draw loops
	struct ActiveEmitter {
		uint32_t containerIdx { 0 };
		uint32_t capacity { 0 };
		uint32_t spawnCount { 0 };
		uint32_t sortSize { 0 };	// power of two draw list size, 0 when the emitter does not need sorting
		int blendMode { 0 };
		bool cullBackFaces { false };
	};

public:
	struct ContainerRefs {
		uint64_t particlesRef { 0 };
		uint64_t aliveListRef { 0 };
		uint64_t deadListRef { 0 };
		uint64_t countersRef { 0 };
	};

	struct ParticlePushConstant {
		uint64_t emitterRef;
		uint64_t containersRef;
		uint64_t drawCommandsRef;
		uint32_t containerIdx;
		uint32_t particleCount;		// spawn count while emitting, capacity while simulating
		float deltaTime;
		float time;
		uint32_t frameSeed;
		uint32_t stage;				// STAGE_* below
		uint32_t sortBlock;			// bitonic sequence size k
		uint32_t sortStride;		// compare distance j
	};

	ParticleRendererVulkan(std::string serviceName = "ParticleRendererVulkan");
	virtual ~ParticleRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	void setSimDeltaTime(float deltaTime) { simDeltaTime = deltaTime; }

protected:
    static constexpr uint32_t MAX_CONTAINERS = 100;
	static constexpr int MAX_PARTICLES_PER_CONTAINER = 1 << 20;
	static constexpr uint32_t STAGE_EMIT = 0;
	static constexpr uint32_t STAGE_SIMULATE = 1;
	static constexpr uint32_t STAGE_SORT_PAD = 2;
	static constexpr uint32_t STAGE_SORT = 3;
	static constexpr uint32_t GROUP_SIZE = 256;
	static constexpr uint32_t QUAD_VERTEX_COUNT = 6;

	std::vector<ContainerRefs> containerRefs { {} };	// attribute refs to one container
	uint32_t containersRefID;
	uint32_t containersVersion { UINT32_MAX };			// ParticleManager version the refs were built from

	std::vector<EmitterGPU> emittersGPU {};				// indexed by container id
	std::vector<uint32_t> emittersRefIDs {};			// one per frame in flight

	// vertex counts are written from the cpu every frame, instance counts by the simulate pass
	std::vector<VkDrawIndirectCommand> drawCommands {};
	uint32_t drawCommandsID { 0 };

	std::vector<ActiveEmitter> activeEmitters {};
	std::unordered_map<uint32_t, MeshRefs> meshRefs {};	// by model id
	std::unordered_set<uint32_t> bindlessTextures {};	// emitter textures already written to the bindless array

	// update ticks and rendered frames are not the same, the sim time is summed per tick and consumed once per render
	float simDeltaTime { 0.0f };
	float simTime { 0.0f };
	uint32_t frameSeed { 0 };
	const float MAX_SIM_STEP = 0.1f;

	virtual void _recreateResources() override;
	virtual void _cleanupResources() override;

	void _createPipelines();
	void _createDescriptor();
	void _updateDescriptor();

	void _prepareEmitter(ParticleEmitter& emitter, bool& deviceIdle);
	void _resolveAssets(ParticleEmitter& emitter);
	void _rebuildContainerRefs();
	void _gatherEmitters(Scene* scene, float deltaTime);
	void _computeParticle(VkCommandBuffer cmd, uint32_t currentFrame);
	void _renderParticle(VkCommandBuffer cmd, uint32_t currentFrame);
	void _uploadContainerRefs();
	void _uploadEmitters(uint32_t frame);
	bool _isValidContainer(uint32_t containerID) const;

	void _sortParticles(VkCommandBuffer cmd);
	VulkanPipeline* _getPipeline(int blendMode, bool cullBackFaces) const;

	std::array<std::unique_ptr<VulkanPipeline>, 4> pipelines;
	std::unique_ptr<VulkanPipeline> computePipeline;
	ParticlePushConstant pushConstant {};

	GeometryPassVulkan* geometryPass { nullptr };
	LightingPassVulkan* lightingPass { nullptr };
	VkFormat depthFormat { VK_FORMAT_UNDEFINED };

	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;
	std::vector<VkDescriptorSet> descriptorSets;

	ParticleManager* particleManager { nullptr };
};
