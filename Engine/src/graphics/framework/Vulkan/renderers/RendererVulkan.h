#pragma once

#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/Vulkan/renderers/RenderGraphVulkan.h"	// passes keep RGHandles of the textures they create
#include <glm/glm.hpp>
#include <vector>

class TextureManagerVulkan;
class RenderDeviceVulkan;
class DescriptorManagerVulkan;
class MaterialManager;
class VulkanPipeline;
class MeshManager;
class ModelManager;
class GuiManager;
class BufferManager;
class RendererManagerVulkan;
class RenderGraphVulkan;
class Camera;

class RendererVulkan : public Renderer
{
public:
	RendererVulkan(std::string name = "RendererVulkan");

	virtual ~RendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) = 0;

	
protected:
	Logger* m_logger { nullptr };
	std::vector<EventSubscription> m_eventSubscriptions;
	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	TextureManagerVulkan* textureManagerVulkan{ nullptr };
	BufferManagerVulkan* bufferManagerVulkan{ nullptr };
	DescriptorManagerVulkan* descriptorManagerVulkan{ nullptr };
	RendererManagerVulkan* rendererManagerVulkan{ nullptr };
	MeshManager* meshManager{ nullptr };
	ModelManager* modelManager{ nullptr };
	MaterialManager* materialManager{ nullptr };
	GuiManager* guiManager{ nullptr };

	bool isActive{ false };
	bool needResize{ false };

	virtual void _recreateResources() = 0;
	virtual void _cleanupResources() = 0;
};

