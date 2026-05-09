#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include <graphics/framework/Vulkan/renderers/renderpasses/ShadowMapPassVulkan.h>
#include "graphics/framework/vulkan/renderers/renderpasses/ImageBasedRendererVulkan.h"

#include <glm/glm.hpp>
#include <vector>

class ForwardRendererVulkan : public RendererVulkan
{

private:
	struct UniformBufferObject {
		glm::mat4 invNormal;
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec4 cameraPos;
		glm::mat4 invView;
		glm::mat4 invProj;
		float width;
		float height;
	};

	struct StorageBufferObject {
		glm::mat4 model;
	};

	struct PushConstantLight {
		uint64_t materialRef;
		float bias;
		float time;
		glm::mat4 sunlightMVP;
		glm::vec4 direction;
		glm::vec4 color;
		float numLights;
		float skyboxDetail;
		uint32_t materialIdx;
	};

	struct LightSSBO {
		alignas(16) glm::vec4 color;
		alignas(16) glm::vec4 position;
		alignas(4) float intensity;
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
	PushConstantLight pushConstantLight;

	std::vector<UniformBufferVulkan*> uniformbuffersList;
	std::vector<StorageBufferVulkan*> storagebuffersList;
	std::vector<StorageBufferVulkan*> lightStoragebuffers;
	std::vector<StorageBufferObject> instanceData;
	std::vector<LightSSBO> lights;
	UniformBufferObject ubo{};

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;

	uint32_t storageBufferID;

	VulkanRenderTarget renderTarget;
	std::unique_ptr<VulkanPipeline> offscreenPipeline;

	ShadowMapPassVulkan* shadowMapRenderer { nullptr };
	ImageBasedRendererVulkan* imageBasedRenderer { nullptr };

	bool isActive{ false };

	float sunIntensity { 10.0f };
	glm::vec4 sunColor { 1.0 };
};

