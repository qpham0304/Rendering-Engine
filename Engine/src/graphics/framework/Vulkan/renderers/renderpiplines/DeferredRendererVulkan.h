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

class DeferredRendererVulkan : public Renderer
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
    DeferredRendererVulkan();
    virtual~DeferredRendererVulkan() override;

    virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;
	virtual void render(Camera& camera) override;

	void beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer);
	void endRecording(void* cmdBuffer);
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);

public:	//TODO: make privat once done testing	
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

	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;

	uint32_t samplerLayoutID;
	uint32_t samplerSetID;
	uint32_t samplerPoolID;

	VulkanRenderTarget renderTarget;
	std::unique_ptr<VulkanPipeline> gPassPipeline;

	uint32_t imGuilayoutID;
	uint32_t imGuipoolID;
	std::vector<uint32_t> imGuisetIDs;

	std::unique_ptr<VulkanPipeline> lightingPipeline;
	uint32_t lightLayoutID;
	uint32_t lightSetsID;

	void _createRenderPasses();
	void _createFrameBuffers();
	void _createDescriptor();
	void _createPipelines();
	void _createViewDescriptorSets();

	void _createLightPipeline();
	void _createLightDescriptor();
};