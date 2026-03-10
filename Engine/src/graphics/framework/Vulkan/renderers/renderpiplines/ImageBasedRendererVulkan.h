#pragma once


class Logger;
class TextureManager;
class MeshManager;
class ModelManager;
class GuiManager;
class BufferManager;
class RenderDeviceVulkan;
class TextureVulkan;
class DescriptorManagerVulkan;
class BufferManagerVulkan;
class MaterialManager;
class VulkanPipeline;
class Camera;

class ImageBasedRendererVulkan
{
public:
    ImageBasedRendererVulkan();
    ~ImageBasedRendererVulkan();

	bool init();
	bool onClose();
	void onUpdate();
	void render(Camera& camera);

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