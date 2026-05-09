#include "RendererManagerVulkan.h"
#include <core/scene/SceneManager.h>


#include "core/features/Timer.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ApplicationRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ForwardRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ShadowMapPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ImageBasedRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/AmbientOcclusionPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/SSRGIPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/TemporalPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/DeferredCombinePassVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "RenderDeviceVulkan.h"
#include "core/features/ServiceLocator.h"

RendererManagerVulkan::RendererManagerVulkan(std::string serviceName)
    : RendererManager(serviceName) 
{
}

RendererManagerVulkan::~RendererManagerVulkan()
{
}

bool RendererManagerVulkan::init(WindowConfig config)
{
    Service::init(config);
	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);

    applicationRenderer = addRenderer<ApplicationRendererVulkan>("ApplicationRendererVulkan");
    forwardRenderer = addRenderer<ForwardRendererVulkan>("ForwardRendererVulkan");
    deferredRenderer = addRenderer<DeferredRendererVulkan>("DeferredRendererVulkan");
    raytracingRenderer = addRenderer<RayTraceRendererVulkan>("RayTraceRendererVulkan");
    shadowMapPass = addRenderer<ShadowMapPassVulkan>("ShadowMapPassVulkan");
    imageBasedRenderer = addRenderer<ImageBasedRendererVulkan>("ImageBasedRendererVulkan");
    alchemyAORenderer = addRenderer<AmbientOcclusionPassVulkan>("AmbientOcclusionPassVulkan");
    hiZPassRenderer = addRenderer<HiZPassVulkan>("HiZPassVulkan");
    SSRGIPassRenderer = addRenderer<SSRGIPassVulkan>("SSRGIPassVulkan");
    temporalPassRenderer = addRenderer<TemporalPassVulkan>("TemporalPassVulkan");
    deferredCombineRenderer = addRenderer<DeferredCombinePassVulkan>("DeferredCombinePassVulkan");
    // postProcessRenderer = addRenderer<PostProcessRendererVulkan>("postProcessRendererRendererVulkan");
	
    applicationRenderer->init(config);
	imageBasedRenderer->init(config);
	shadowMapPass->init(config);
    // alchemyAORenderer->init(config);
    // hiZPassRenderer->init(config);
    forwardRenderer->init(config);
	deferredRenderer->init(config);
	raytracingRenderer->init(config);
    SSRGIPassRenderer->init(config);
    temporalPassRenderer->init(config);
    deferredCombineRenderer->init(config);
	// postProcessRenderer->init(config);

    return true;
}

bool RendererManagerVulkan::onClose()
{
    Service::onClose();

    // for(auto& [name, renderer] : m_renderers) {
    //     renderer->onClose();
    // }
    renderDeviceVulkan->waitIdle();
    applicationRenderer->onClose();
	forwardRenderer->onClose();
	deferredRenderer->onClose();
	raytracingRenderer->onClose();
	shadowMapPass->onClose();
	imageBasedRenderer->onClose();
    alchemyAORenderer->onClose();
    hiZPassRenderer->onClose();
    SSRGIPassRenderer->onClose();
    temporalPassRenderer->onClose();
    deferredCombineRenderer->onClose();
	// postProcessRenderer->onClose();
    return true;
}

void RendererManagerVulkan::destroy(uint32_t id)
{

}

std::vector<uint32_t> RendererManagerVulkan::listIDs() const
{
    return std::vector<uint32_t>();
}

void RendererManagerVulkan::onUpdate()
{
    if(currentRenderMode == 0) {
        forwardRenderer->onUpdate();
    } 
    else if(currentRenderMode == 1) {
        auto tmp = (DeferredRendererVulkan*)deferredRenderer;
        if(tmp->pushConstantLight.aoOn) {
            alchemyAORenderer->onUpdate();
        }
        deferredRenderer->onUpdate();
        hiZPassRenderer->onUpdate();
        SSRGIPassRenderer->onUpdate();
        if(tmp->denoiserOn) {
            temporalPassRenderer->onUpdate();
        }
        deferredCombineRenderer->onUpdate();
    } else if(currentRenderMode == 2) {
        raytracingRenderer->onUpdate();
    }
}

void RendererManagerVulkan::render()
{
    Timer timer("Renderer Manager", true);
    Scene* scene = SceneManager::getInstance().getActiveScene();
    Camera* camera = SceneManager::cameraController;

	if(!SceneManager::cameraController) {
		return;
	}

    beginFrame();
    // shadowMapRenderer->render(*camera);
    
    if(currentRenderMode == 0) {
        forwardRenderer->render(*camera);
    } 
    else if(currentRenderMode == 1) {
        auto tmp = (DeferredRendererVulkan*)deferredRenderer;
        if(tmp->pushConstantLight.aoOn) {
            alchemyAORenderer->render(*camera);
        }
        deferredRenderer->render(*camera);
        hiZPassRenderer->render(*camera);
        SSRGIPassRenderer->render(*camera);
        if(tmp->denoiserOn) {
            temporalPassRenderer->render(*camera);
        }
        deferredCombineRenderer->render(*camera);
    } else if(currentRenderMode == 2) {
        raytracingRenderer->render(*camera);
    }

    applicationRenderer->render(*camera);
    endFrame();
}

RendererVulkan* RendererManagerVulkan::getRenderer(std::string_view name)
{
    auto it = m_renderers.find(name.data());
    if(it == m_renderers.end()) {
        return nullptr;
    }
    return dynamic_cast<RendererVulkan*>(it->second.get());
}

void RendererManagerVulkan::setRenderMode(uint32_t mode)
{
    currentRenderMode = mode;
}

int RendererManagerVulkan::getRenderMode()
{
    return currentRenderMode;
}

void RendererManagerVulkan::beginFrame()
{
	renderDeviceVulkan->beginFrame();
	renderDeviceVulkan->commandPool.beginBuffer();
}


void RendererManagerVulkan::endFrame()
{
	renderDeviceVulkan->commandPool.endBuffer();
	renderDeviceVulkan->endFrame();
}

void RendererManagerVulkan::setDisplayImage(TextureVulkan* texture)
{
    displayImage = texture;
}

TextureVulkan* RendererManagerVulkan::getDisplayImage()
{
    return displayImage;
}