#include "DeferredRendererVulkan.h"
#include <window/AppWindow.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/scene/SceneManager.h>
#include <core/features/ServiceLocator.h>
#include <core/events/EventManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/renderers/RenderDevice.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/AmbientOcclusionPassVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/SSRGIPassVulkan.h>
#include <graphics/framework/vulkan/renderers/features/DDGIBuilderVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h>

DeferredRendererVulkan::DeferredRendererVulkan()
	: RendererVulkan("DeferredRendererVulkan")
{
	setFootprint("DeferredRendererVulkan", sizeof(DeferredRendererVulkan), MemorySystem::Graphics);

}

DeferredRendererVulkan::~DeferredRendererVulkan()
{

}

bool DeferredRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	RendererVulkan* renderer = nullptr;
	renderer = rendererManagerVulkan->getRenderer("ShadowMapPassVulkan");
	shadowMapPass = dynamic_cast<ShadowMapPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("AmbientOcclusionPassVulkan");
	ambientOcclusionPass = dynamic_cast<AmbientOcclusionPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	hiZPass = dynamic_cast<HiZPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("GeometryPassVulkan");
	geometryPass = dynamic_cast<GeometryPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("LightingPassVulkan");
	lightingPass = dynamic_cast<LightingPassVulkan*>(renderer);

	assert(shadowMapPass
		&& imageBasedRenderer
		&& ambientOcclusionPass
		&& hiZPass
		&& geometryPass
		&& lightingPass
		&& "failed to retrieve renderer"
	);

	// TODO: add a depth prepass to fix this 
	// the geometry pass is registered before this renderer so the g-buffer exists already,
	// ao has to exist before the lighting pass (registered after this) builds its descriptors
	ambientOcclusionPass->init(config);
	hiZPass->init(config);

	settings = &rendererManagerVulkan->getGraphicsSettings().add<DeferredSettings>("Deferred");

	return true;
}

bool DeferredRendererVulkan::onClose()
{
	rendererManagerVulkan->getGraphicsSettings().remove<DeferredSettings>();
	settings = nullptr;

	return true;
}

void DeferredRendererVulkan::onUpdate()
{
	// the targets are recreated on resize, publish the current ones for the debug views
	auto& gBuffer = geometryPass->renderTarget;
	settings->litColor = lightingPass->renderTarget.colorTexture->id();
	settings->position = gBuffer.gBufferPos->id();
	settings->normal = gBuffer.gBufferNorm->id();
	settings->albedo = gBuffer.gBufferAlbedo->id();
	settings->pbr = gBuffer.gPBR->id();
	settings->depth = gBuffer.depthTexture->id();
	settings->motion = gBuffer.gBufferMotion->id();
	settings->shadowDepth = shadowMapPass->depthID;
}

void DeferredRendererVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);
}

void DeferredRendererVulkan::_recreateResources()
{

}

void DeferredRendererVulkan::_cleanupResources()
{

}

void DeferredRendererVulkan::addToGraph(RenderGraphVulkan& graph)
{

}
