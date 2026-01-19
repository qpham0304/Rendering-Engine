#pragma once

#include "graphics/renderers/Renderer.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include "graphics/framework/vulkan/core/WrapperStructs.h"
#include "../resources/buffers/BufferManagerVulkan.h"

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

class RendererVulkan : public Renderer
{
public:
	struct RenderTarget {
		VkRenderPass renderPass;
		std::vector<VkFramebuffer> framebuffers;
		std::vector<TextureVulkan*> colorTextures;
		std::vector<TextureVulkan*> depthTextures;

		void destroy(VkDevice device) {
			for (size_t i = 0; i < framebuffers.size(); i++) {
				vkDestroyFramebuffer(device, framebuffers[i], nullptr);
			}
			vkDestroyRenderPass(device, renderPass, nullptr);
		}
	};

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
	RendererVulkan(std::string serviceName = "RendererVulkan");

	virtual ~RendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;
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

	//TODO: PRIORITY: support recreation on screen resize
	void _createOffscreenViewDescriptorSet();
	void _createDepthResources(VulkanDevice& device, TextureVulkan& depthTexture);

private:
	const int numInstances = 1;
	const int numLights = 100;

	bool showGui{ true };
	PushConstantData pushConstantData{};

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

	RenderTarget renderTarget;
	std::unique_ptr<VulkanPipeline> offscreenPipeline;
	uint32_t imGuilayoutID;
	uint32_t imGuipoolID;
	std::vector<uint32_t> imGuisetIDs;
};

