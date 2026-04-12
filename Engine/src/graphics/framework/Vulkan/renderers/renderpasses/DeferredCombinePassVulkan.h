#pragma once

#include "PostProcessRendererVulkan.h"
#include <vulkan/vulkan.h>

class DeferredCombinePassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstant {
        alignas(8) glm::vec2 screenRes;
		alignas(4) float time;
		alignas(4) float frameSeed;
	};

	struct UniformBufferObject {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 invProj;
		glm::mat4 invView;
	};

public:
	DeferredCombinePassVulkan(std::string serviceName = "DeferredCombinePassVulkan");
	virtual ~DeferredCombinePassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

    void writeCombinedImage(VkCommandBuffer cmd, uint32_t currentFrame);

public: // TODO: consider make private
	TextureVulkan* denoisedGIImage;
	TextureVulkan* sceneImage;
	TextureVulkan* albedoImage;

protected:
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _recreateResources();
	void _cleanupResources();

	UniformBufferObject ubo;
	PushConstant pushConstant;
	std::vector<UniformBufferVulkan*> uniformbuffersList;

};