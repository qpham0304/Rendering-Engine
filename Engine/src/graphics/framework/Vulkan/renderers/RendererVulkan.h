#pragma once

#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/WrapperStructs.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
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

class RendererVulkan : public Renderer, protected VkWrap
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

public:
	RendererVulkan(std::string serviceName = "RendererVulkan");

	virtual ~RendererVulkan() override;

	virtual int init(WindowConfig config) override;
	virtual int onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;
	virtual void render(Camera& camera, Scene* scene) override;
	virtual void addMesh() override;
	virtual void addModel(std::string_view path) override;

	void beginRecording(void* cmdBuffer);
	void endRecording(void* cmdBuffer);

public:
	
private:
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void renderGui(void* commandBuffer);

	void _createDescriptorSetLayout();
	void _createDescriptorPool();
	void _createDescriptorSets();
	//void _createSSBO();

private:
	const int numInstances = 1;
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
	std::vector<StorageBufferObject> instanceData;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;


	uint32_t storageBufferID;
};

