#pragma once

#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/WrapperStructs.h"
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

class RendererVulkan : public Renderer, protected VkWrap
{
private:
    struct PushConstantData {
        alignas(16) glm::vec3 color;
        alignas(16) glm::vec3 range;
        alignas(4)  bool flag;
        alignas(4)  float data;
    };


public:
	RendererVulkan();

	virtual ~RendererVulkan() override;

	virtual int init(WindowConfig config) override;
	virtual int onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;
	virtual void render(Camera& camera, Scene* scene) override;
	virtual void shutdown() override;
	virtual void addMesh() override;
	virtual void addModel(std::string_view path) override;

	void beginRecording(void* cmdBuffer);
	void endRecording(void* cmdBuffer);

public:
	//TODO: does not have factory to register yet so make public so client can access it for now
    BufferManager* bufferManager { nullptr };
	
private:
	void recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void renderGui(void* commandBuffer);

private:
	Logger* m_logger;
	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	TextureManager* textureManager{ nullptr };
	MeshManager* meshManager { nullptr };
	ModelManager* modelManager { nullptr };
	GuiManager* guiManager { nullptr };
	bool showGui{ true };
	
	PushConstantData pushConstantData{};

	uint32_t modelID;

};

