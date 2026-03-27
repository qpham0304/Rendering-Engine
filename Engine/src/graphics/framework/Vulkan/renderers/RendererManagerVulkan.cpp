#include "RendererManagerVulkan.h"
#include <core/scene/SceneManager.h>


#include "graphics/framework/vulkan/renderers/renderpiplines/ApplicationRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ForwardRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ShadowMapRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ImageBasedRendererVulkan.h"
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
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
    shadowMapRenderer = addRenderer<ShadowMapRendererVulkan>("ShadowMapRendererVulkan");
    // imageBasedRenderer = addRenderer<ImageBasedRendererVulkan>("ImageBasedRendererVulkan");
    // postProcessRenderer = addRenderer<PostProcessRendererVulkan>("postProcessRendererRendererVulkan");
	
    applicationRenderer->init(config);
    forwardRenderer->init(config);
	deferredRenderer->init(config);
	shadowMapRenderer->init(config);
	// imageBasedRenderer->init(config);
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
	shadowMapRenderer->onClose();
	// imageBasedRenderer->onClose();
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
    
}

void RendererManagerVulkan::render()
{
    Scene* scene = SceneManager::getInstance().getActiveScene();
    Camera* camera = SceneManager::cameraController;

	if(!SceneManager::cameraController) {
		return;
	}

    beginFrame();
    shadowMapRenderer->render(*camera);
	forwardRenderer->render(*camera);
    // deferredRenderer->render(*camera);
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