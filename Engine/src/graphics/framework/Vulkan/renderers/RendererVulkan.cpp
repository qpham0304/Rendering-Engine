#include "RendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "graphics/renderers/RenderDevice.h"
#include "RenderDeviceVulkan.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <gui/GuiManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <core/scene/SceneManager.h>

RendererVulkan::RendererVulkan(std::string serviceName) 
	:	Renderer(serviceName)
{

}

RendererVulkan::~RendererVulkan()
{

}

bool RendererVulkan::init(WindowConfig config)
{
	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerSPD");

	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	bufferManagerVulkan = &dynamic_cast<BufferManagerVulkan&>(bufferManager);
	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = &dynamic_cast<DescriptorManagerVulkan&>(descriptorManager);
	TextureManager& textureManager = ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
	textureManagerVulkan = &dynamic_cast<TextureManagerVulkan&>(textureManager);
	RendererManager& rendererManager = ServiceLocator::GetService<RendererManager>("RendererManagerVulkan");
	rendererManagerVulkan = &dynamic_cast<RendererManagerVulkan&>(rendererManager);

	meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
	modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
	guiManager = &ServiceLocator::GetService<GuiManager>("ImGuiManager");

	if(!(
		renderDeviceVulkan &&
		bufferManagerVulkan &&
		descriptorManagerVulkan &&
		textureManagerVulkan &&
		rendererManagerVulkan &&
		meshManager &&
		materialManager &&
		modelManager &&
		guiManager
	)) {
		return false;
	}


	return true;
}

bool RendererVulkan::onClose()
{
	return true;
}

void RendererVulkan::onUpdate()
{
	
}

void RendererVulkan::render(Camera& camera)
{

}