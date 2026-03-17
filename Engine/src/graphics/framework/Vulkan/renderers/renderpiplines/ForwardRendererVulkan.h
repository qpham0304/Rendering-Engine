#pragma once

#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/RendererVulkan.h"

#include <glm/glm.hpp>
#include <vector>

class ForwardRendererVulkan : public RendererVulkan
{
public:

private:
    struct PushConstantData {
        alignas(16) glm::vec3 color;
        alignas(16) glm::vec3 range;
        alignas(4)  bool flag;
        alignas(4)  float data;
    };
	
	struct UniformBufferObject {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	struct StorageBufferObject {
		glm::mat4 model;
	};

	struct LightSSBO {
		glm::vec4 color = glm::vec4(1.0f);
		int modelIndex;
		float intensity;
	};

public:
	ForwardRendererVulkan(std::string serviceName = "ForwardRendererVulkan");

	virtual ~ForwardRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

	void beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer);
	void endRecording(void* cmdBuffer);

public:
	
private:
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void recordDrawToTextureCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void renderGui(void* commandBuffer);


	void _createPipeline();
	void _createOffscreenTarget();
	void _createDescriptorSetLayout();
	void _createDescriptorPool();
	void _createDescriptorSets();
	void _updateDescriptor();
	void _recreateResources();
	void _cleanupResources();

private:
	const int numInstances = 1;
	const int numLights = 100;

	bool showGui{ true };
	PushConstantData pushConstantData{};

	std::vector<UniformBufferVulkan*> uniformbuffersList;
	std::vector<StorageBufferVulkan*> storagebuffersList;
	std::vector<StorageBufferVulkan*> lightStoragebuffers;
	std::vector<StorageBufferObject> instanceData;
	std::vector<LightSSBO> lights;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;

	uint32_t storageBufferID;

	VulkanRenderTarget renderTarget;
	std::unique_ptr<VulkanPipeline> offscreenPipeline;

	bool isActive{ false };

	DeferredRendererVulkan* deferredRenderer;
};

