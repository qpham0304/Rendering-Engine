#pragma once
#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
 

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
class Camera;

class ImageBasedRendererVulkan : public Renderer
{
public:
    ImageBasedRendererVulkan();
    virtual ~ImageBasedRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override {};
	virtual void endFrame() override {};
	virtual void render(Camera& camera) override;

protected:
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

private:

};