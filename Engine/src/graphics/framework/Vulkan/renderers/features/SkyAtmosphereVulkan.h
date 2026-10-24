#pragma once
#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

#include <glm/glm.hpp>

// Hillaire 2020 precomputed atmosphere: transmittance and multiple scattering luts, then the sky seen from the
// viewer as an equirect environment. the image based pass takes that as its environment, so the light pass
class SkyAtmosphereVulkan : public RendererVulkan
{
public:
	struct AtmosphereParams {
		glm::vec4 rayleighScattering;
		glm::vec4 mie;
		glm::vec4 ozoneAbsorption;
		glm::vec4 groundAlbedo;
		glm::vec4 sunDirection;
		glm::vec4 sunIlluminance;
		float bottomRadius;
		float topRadius;
		float viewHeight;
		float padding;
	};

public:
	SkyAtmosphereVulkan(std::string serviceName = "SkyAtmosphereVulkan");
	virtual ~SkyAtmosphereVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	TextureVulkan* getEnvironmentImage() { return environmentMap; }

protected:
	SkyAtmosphereSettings* settings { nullptr };
	AtmosphereParams params {};
	AtmosphereParams lastParams {};
	bool lutDirty { true };
	bool environmentDirty { true };
	bool overrideActive { false };

	TextureVulkan* transmittanceLut { nullptr };
	TextureVulkan* multiScatteringLut { nullptr };
	TextureVulkan* environmentMap { nullptr };
	RGHandle transmittanceHandle {};
	RGHandle multiScatteringHandle {};
	RGHandle environmentHandle {};
	uint32_t transmittanceID { 0 };
	uint32_t multiScatteringID { 0 };
	uint32_t environmentID { 0 };

	std::unique_ptr<VulkanPipeline> transmittancePipeline;
	std::unique_ptr<VulkanPipeline> multiScatteringPipeline;
	std::unique_ptr<VulkanPipeline> environmentPipeline;
	uint32_t transmittanceLayoutID;
	uint32_t multiScatteringLayoutID;
	uint32_t environmentLayoutID;
	uint32_t descriptorPoolID;
	uint32_t transmittanceSetID;
	uint32_t multiScatteringSetID;
	uint32_t environmentSetID;

	void _updateParams();
	void _dispatch(VkCommandBuffer cmd, VulkanPipeline* pipeline, uint32_t setID, TextureVulkan* target);
	void _createResources();
	void _createDescriptors();
	void _updateDescriptors();
	void _createPipelines();
	void _recreateResources() override;
	void _cleanupResources() override;
};
