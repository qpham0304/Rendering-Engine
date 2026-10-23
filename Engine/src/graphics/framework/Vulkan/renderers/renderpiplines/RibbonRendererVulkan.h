#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include <array>
#include <random>
#include <unordered_set>

class GeometryPassVulkan;
class LightingPassVulkan;
struct RibbonComponent;

// camera facing strips through point lists (trails, beams, lightning) and swept blade surfaces, drawn into the lit scene after particles
class RibbonRendererVulkan : public RendererVulkan
{
private:
	// mirrored by RibbonPoint in ribbon.vert (scalar layout)
	struct RibbonPointGPU {
		glm::vec3 position { 0.0f };
		float width { 0.0f };
		glm::vec4 color { 1.0f };
		float u { 0.0f };
	};
	static_assert(sizeof(RibbonPointGPU) == 36, "RibbonPointGPU must match RibbonPoint in ribbon.vert");

	struct RibbonPushConstant {
		uint64_t pointsRef;
		uint32_t firstPoint;
		uint32_t pointCount;
		int32_t textureID;
		uint32_t flags;
	};

	// mirrored in ribbon.vert and ribbon.frag
	static constexpr uint32_t STRIP_FLAG_PAIRED = 1u;		// points come in edge pairs, the strip is not camera facing

	// one strip of this frame's shared point buffer
	struct StripDraw {
		uint32_t firstPoint { 0 };
		uint32_t pointCount { 0 };		// pairs for a paired strip
		int textureID { 0 };
		int blendMode { 0 };
		uint32_t flags { 0 };
	};

	// how one strip is shaded along its length
	struct StripStyle {
		float widthScale { 1.0f };
		float alphaScale { 1.0f };
		float brightness { 1.0f };
		const std::vector<float>* ages { nullptr };		// trail point ages, faded by the trail lifetime
	};

public:
	RibbonRendererVulkan(std::string serviceName = "RibbonRendererVulkan");
	virtual ~RibbonRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	// ribbons run on real time so trails and lightning also animate in the editor while the game is stopped
	void setDeltaTime(float deltaTime) { this->deltaTime = deltaTime; }

protected:
	static constexpr uint32_t MAX_POINTS = 16384;
	static constexpr int MAX_SEGMENTS = 256;

	void _updateTrail(RibbonComponent& ribbon, const glm::vec3& head, float dt);
	void _updateLightning(RibbonComponent& ribbon, const glm::vec3& start, const glm::vec3& end, float dt);
	void _updateBlade(RibbonComponent& ribbon, const glm::vec3& base, const glm::vec3& tip, float dt);
	void _appendBlade(const RibbonComponent& ribbon, const glm::vec3& base, const glm::vec3& tip);
	std::vector<glm::vec3> _generateBolt(const glm::vec3& start, const glm::vec3& end, int segments, float jitter, std::mt19937& rng) const;
	void _appendStrip(const std::vector<glm::vec3>& positions, const RibbonComponent& ribbon, const StripStyle& style);

	void _createPipelines();
	void _createDescriptor();

	virtual void _recreateResources() override;
	virtual void _cleanupResources() override;

	std::vector<RibbonPointGPU> points {};
	std::vector<StripDraw> draws {};
	std::vector<uint32_t> pointBufferIDs {};		// one per frame in flight
	std::unordered_set<uint32_t> bindlessTextures {};

	float deltaTime { 0.0f };
	float time { 0.0f };

	std::array<std::unique_ptr<VulkanPipeline>, 2> pipelines;	// indexed by ParticleBlendMode
	RibbonPushConstant pushConstant {};

	GeometryPassVulkan* geometryPass { nullptr };
	LightingPassVulkan* lightingPass { nullptr };
	VkFormat depthFormat { VK_FORMAT_UNDEFINED };

	uint32_t layoutID { 0 };
	uint32_t poolID { 0 };
	uint32_t setsID { 0 };
	std::vector<VkDescriptorSet> descriptorSets;
};
