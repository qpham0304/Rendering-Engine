#pragma once

#include "PostProcessRendererVulkan.h"
#include <vulkan/vulkan.h>

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

    void writeHiZ(VkCommandBuffer cmd, uint32_t currentFrame);

	//TODO: turn this into private
	// TextureVulkan* hiZImage;
	std::vector<TextureVulkan*> outputImages;

protected:
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _recreateResources();
	void _cleanupResources();

	TextureVulkan* depthImage;
	TextureVulkan* normalImage;

	uint32_t hiZLayoutID;
	uint32_t hiZPoolID;
	uint32_t hiZSetsID;
	uint32_t mipLevels;
	uint32_t mapSize{ 1024 };
	// std::vector<VkImageView> HiZMipViews;
	std::vector<std::vector<VkImageView>> HiZMipViews;
	std::vector<std::vector<VkDescriptorSet>> HiZMipSets;
	PushConstant pushConstant;
};