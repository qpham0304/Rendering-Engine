#pragma once

#include "PostProcessRendererVulkan.h"
#include <vulkan/vulkan.h>

class SSRGIPassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstant {
        alignas(8) glm::vec2 screenRes;
        alignas(4) float maxDistance;
        alignas(4) int maxMip;
		alignas(4) float thickness;
		alignas(4) float time;
	};

	struct UniformBufferObject {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 invProj;
		glm::mat4 invView;
	};

public:
	SSRGIPassVulkan(std::string serviceName = "SSRGIPassVulkan");
	virtual ~SSRGIPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

    void writeSSRGI(VkCommandBuffer cmd, uint32_t currentFrame);

protected:
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _recreateResources();
	void _cleanupResources();

	TextureVulkan* depthImageHiZ;
	TextureVulkan* depthImageRaw;
	TextureVulkan* normalImage;
	TextureVulkan* albedoImage;
	TextureVulkan* pbrImage;
	TextureVulkan* emissiveImage;
	TextureVulkan* colorImage;

	uint32_t SSRGILayoutID;
	uint32_t SSRGIPoolID;
	uint32_t SSRGISetsID;
	UniformBufferObject ubo;
	PushConstant pushConstant;
	std::vector<UniformBufferVulkan*> uniformbuffersList;

};