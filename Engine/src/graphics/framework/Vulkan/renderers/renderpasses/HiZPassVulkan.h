#pragma once

#include "PostProcessRendererVulkan.h"
#include <volk.h>

class HiZPassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstant {
    	alignas(8) glm::vec2 u_PreviousLevelDimensions;
		alignas(4) int u_MipLevel;
	};

public:
	HiZPassVulkan(std::string serviceName = "HiZPassVulkan");
	virtual ~HiZPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

    void writeHiZ(VkCommandBuffer cmd, uint32_t currentFrame);

	//TODO:make these private
	TextureVulkan* hizImage { nullptr };
	RGHandle hizHandle {};

protected:
	void _createResources();
	void _createPipelines();
	void _createDescriptorLayout();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _recreateResources();
	void _cleanupResources();

	TextureVulkan* depthImage { nullptr };
	RGHandle depthHandle {};

	uint32_t hiZLayoutID;
	uint32_t hiZPoolID;
	uint32_t hiZSetsID;
	uint32_t mipLevels;
	uint32_t width { 0 };
	uint32_t height { 0 };
	bool hizRecreated { false };

	std::vector<VkImageView> HiZMipViews;
	std::vector<VkDescriptorSet> HiZMipSets;
	PushConstant pushConstant;
};