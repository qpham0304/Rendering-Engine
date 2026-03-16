#pragma once

#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

#include <glm/glm.hpp>
#include <vector>

class ApplicationRendererVulkan : public RendererVulkan
{

public:
	ApplicationRendererVulkan(std::string serviceName = "ApplicationRendererVulkan");

	virtual ~ApplicationRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

	void beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer, void* pipeline);
	void endRecording(void* cmdBuffer);

public:
	
private:
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void recordDrawToTextureCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void renderGui(void* commandBuffer);

	void _createOffscreenTarget();
	void _createDescriptorSetLayout();
	void _createDescriptorPool();
	void _createDescriptorSets();

private:
	bool showGui{ true };
	bool isActive{ false };

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;

	uint32_t storageBufferID;

	VulkanRenderTarget renderTarget;
	std::unique_ptr<VulkanPipeline> offscreenPipeline;
};

