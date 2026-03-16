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
	ApplicationRendererVulkan(std::string serviceName = "ApplicationRendererVulkan");

	virtual ~ApplicationRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

	void beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer);
	void endRecording(void* cmdBuffer);

private:
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void renderGui(void* commandBuffer);

	void _createDescriptors();
	void _updateDescriptorSets(uint32_t index);

private:
	const int numInstances = 1;
	const int numLights = 100;

	bool showGui{ true };
	PushConstantData pushConstantData{};
	
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;

	uint32_t storageBufferID;
	std::unique_ptr<VulkanPipeline> appPipeline;

	bool isActive{ false };
};

