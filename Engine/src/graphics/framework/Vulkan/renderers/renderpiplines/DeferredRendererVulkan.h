#pragma once

#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "ShadowMapRendererVulkan.h"

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

class DeferredRendererVulkan : public Renderer
{
private:
	struct UniformBufferObject {
		glm::mat4 invNormal;
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec4 cameraPos;
	};

	struct StorageBufferObject {
		glm::mat4 model;
	};

	struct PushConstantLight {
		alignas(64) glm::mat4 sunlightMVP;
		alignas(16) glm::vec4 direction;
		alignas(16) glm::vec4 color;
		alignas(4)  int numLights;
	};

	struct LightSSBO {
		alignas(16) glm::vec4 color;
		alignas(16) glm::vec4 position;
		alignas(4) float intensity;
		alignas(4) float radius;
	};

public:
    DeferredRendererVulkan();
    virtual~DeferredRendererVulkan() override;

    virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;
	virtual void render(Camera& camera) override;
	void renderGui();

	void beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer);
	void endRecording(void* cmdBuffer);
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);

public:	//TODO: make private once done testing	
	const int numInstances = 1;
	const int numLights = 1000;

	bool showGui{ true };

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
	std::vector<StorageBufferObject> instanceData;
	std::vector<StorageBufferVulkan*> lightStoragebuffers;
	std::vector<LightSSBO> lights;
	PushConstantLight pushConstantLight;

	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;

	VulkanRenderTarget renderTarget;
	std::unique_ptr<VulkanPipeline> gPassPipeline;

	uint32_t imGuilayoutID;
	uint32_t imGuipoolID;
	std::vector<uint32_t> imGuisetIDs;

	std::unique_ptr<VulkanPipeline> lightingPipeline;
	uint32_t lightLayoutID;
	uint32_t lightLayoutID_1;
	uint32_t lightSetsID;
	uint32_t lightSetsID_1;
	UniformBufferObject ubo{};


	ShadowMapRendererVulkan shadowMapRenderer;

	void _createRenderPasses();
	void _createFrameBuffers();
	void _createDescriptor();
	void _createPipelines();
	void _createViewDescriptorSets();

	void _createLightPipeline();
	void _createLightDescriptor();

};