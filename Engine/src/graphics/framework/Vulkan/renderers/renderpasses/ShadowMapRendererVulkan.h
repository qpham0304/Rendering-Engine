#pragma once

#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

class ShadowMapRendererVulkan : public RendererVulkan
{
private:
	struct LightPushConstant {
		glm::mat4 model;
		glm::mat4 lightMVP;
	};

	struct ComputePushConstant {
		uint32_t isVertical;	// 0 for horizontal, 1 for vertical
		uint32_t radius;
		float sigma;
	};

public:
	ShadowMapRendererVulkan();
	virtual~ShadowMapRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

	void beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer, void* pipeline);
	void endRecording(void* cmdBuffer);
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void dispatchBlur(VkCommandBuffer commandBuffer, uint32_t imageIndex);

public:	
	std::unique_ptr<VulkanPipeline> shadowPipeline;
	uint32_t depthID;
	TextureVulkan* depthMap;
	TextureVulkan* momentImage;
	TextureVulkan* tempMomentImage;
	TextureVulkan* blueNoiseImage;
	VkRenderPass shadowRenderPass;
	VkFramebuffer shadowFramebuffer;
	float sunAzimuth = 0.0f;   // Horizontal rotation
	float sunElevation = 0.5f; // Vertical height

	//TODO: allow client spcify the shadow map size
	uint32_t width = 1024;
	uint32_t height = 1024;

	glm::mat4 lightSpaceMatrix;
	glm::vec3 lightPos;
	glm::vec3 lightDir;
	glm::mat4 lightView;
	float s;
	float zNear;
	float zFar;

	std::unique_ptr<VulkanPipeline> computePipeline;
	uint32_t compDescriptorLayoutID;
	uint32_t compDescriptorPoolID;
	uint32_t compDescSetMtoT_ID;
	uint32_t compDescSetTtoM_ID;

	ComputePushConstant pushconstant;
	bool useOrtho = true;

private:
	void _createDepthMap();
	void _createShadowPipeline();
	void _createShadowRenderPass();
	void _createShadowFrameBuffer();
	void _createMomentImage();
	void _createMomentDescriptor();
	// void _updateMomentDesccriptor();
	void _createComputePipeline();
};