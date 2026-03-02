#pragma once

#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>


class Logger;
class TextureManager;
class MeshManager;
class ModelManager;
class GuiManager;
class BufferManager;
class RenderDeviceVulkan;
class TextureVulkan;
class DescriptorManagerVulkan;
class MaterialManager;
class VulkanPipeline;

class ShadowMapRendererVulkan : public Renderer
{
private:
	struct LightPushConstant {
		glm::mat4 model;
		glm::mat4 lightMVP;
	};

	struct ComputePushConstant {
		uint32_t isVertical;	// 0 for horizontal, 1 for vertical
	};

public:
	ShadowMapRendererVulkan();
	virtual~ShadowMapRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;
	virtual void render(Camera& camera) override;

	void beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer, void* pipeline);
	void endRecording(void* cmdBuffer);
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void dispatchBlur(VkCommandBuffer commandBuffer, uint32_t imageIndex);

public:	//TODO: make private once done testing	
	Logger* m_logger{ nullptr };
	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	MeshManager* meshManager{ nullptr };
	ModelManager* modelManager{ nullptr };
	GuiManager* guiManager{ nullptr };
	TextureManager* textureManager{ nullptr };
	MaterialManager* materialManager{ nullptr };
	BufferManager* bufferManager{ nullptr };
	BufferManagerVulkan* bufferManagerVulkan{ nullptr };
	DescriptorManagerVulkan* descriptorManagerVulkan{ nullptr };


	std::unique_ptr<VulkanPipeline> shadowPipeline;
	//uint32_t shadowLayoutID;
	//uint32_t shadowPoolID;
	//uint32_t shadowSetsID;
	TextureVulkan* depthMap;
	TextureVulkan* momentImage;
	TextureVulkan* tempMomentImage;
	VkRenderPass shadowRenderPass;
	VkFramebuffer shadowFramebuffer;

	//TODO: allow client spcify the shadow map size
	uint32_t width = 4096;
	uint32_t height = 4096;

	glm::mat4 lightSpaceMatrix;
	glm::vec3 lightPos;
	glm::mat4 lightView;

	std::unique_ptr<VulkanPipeline> computePipeline;
	uint32_t compDescriptorLayoutID;
	uint32_t compDescriptorPoolID;
	uint32_t compDescSetMtoT_ID;
	uint32_t compDescSetTtoM_ID;

	uint32_t imGuilayoutID;
	uint32_t imGuipoolID;
	std::vector<uint32_t> imGuisetIDs;
	VkDescriptorSet imGuiDescriptorSet;

private:
	void _createDepthMap();
	void _createShadowPipeline();
	void _createShadowRenderPass();
	void _createShadowFrameBuffer();
	void _createMomentImage();
	void _createMomentDescriptor();
    void _createOffscreenViewDescriptorSet();
	void _createComputePipeline();
};