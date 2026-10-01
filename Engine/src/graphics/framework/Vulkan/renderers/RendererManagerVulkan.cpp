#include "RendererManagerVulkan.h"
#include <core/scene/SceneManager.h>

#include "core/features/Timer.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ApplicationRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ForwardRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ParticleRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ShadowMapPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/AmbientOcclusionPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/SSRGIPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/BloomPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/TemporalPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/DeferredCombinePassVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/features/ImageBasedVulkan.h"
#include "graphics/framework/vulkan/renderers/features/DDGIBuilderVulkan.h"
#include "RenderDeviceVulkan.h"
#include "core/features/ServiceLocator.h"
#include "core/features/camera.h"
#include "core/events/EventManager.h"

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
    
	EventManager::getInstance().subscribe(EventType::WindowResize, [this] (Event& event) {
		this->needResize = true;
	});
    
	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	BufferManagerVulkan* bufferManagerVulkan = &dynamic_cast<BufferManagerVulkan&>(bufferManager);
    bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));


    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;
	ubo.width = swapchain.swapChainExtent.width;    //TODO: check if this update properly
	ubo.height = swapchain.swapChainExtent.height;

    applicationRenderer = addRenderer<ApplicationRendererVulkan>("ApplicationRendererVulkan");
    
    shadowMapPass = addRenderer<ShadowMapPassVulkan>("ShadowMapPassVulkan");
    imageBasedRenderer = addRenderer<ImageBasedRendererVulkan>("ImageBasedRendererVulkan");

    forwardRenderer = addRenderer<ForwardRendererVulkan>("ForwardRendererVulkan");
    deferredRenderer = addRenderer<DeferredRendererVulkan>("DeferredRendererVulkan");
    raytracingRenderer = addRenderer<RayTraceRendererVulkan>("RayTraceRendererVulkan");
    
    alchemyAORenderer = addRenderer<AmbientOcclusionPassVulkan>("AmbientOcclusionPassVulkan");
    hiZPassRenderer = addRenderer<HiZPassVulkan>("HiZPassVulkan");
    SSRGIPassRenderer = addRenderer<SSRGIPassVulkan>("SSRGIPassVulkan");
    bloomRenderer = addRenderer<BloomPassVulkan>("BloomPassVulkan");
    temporalPassRenderer = addRenderer<TemporalPassVulkan>("TemporalPassVulkan");
    ddgiPassRenderer = addRenderer<DDGIBuilderVulkan>("DDGIBuilderVulkan");
    deferredCombineRenderer = addRenderer<DeferredCombinePassVulkan>("DeferredCombinePassVulkan");
    // postProcessRenderer = addRenderer<PostProcessRendererVulkan>("postProcessRendererVulkan");
    particleRenderer = addRenderer<ParticleRendererVulkan>("ParticleRendererVulkan");
	
    // applicationRenderer->init(config);
	// imageBasedRenderer->init(config);
	// shadowMapPass->init(config);
    // // // alchemyAORenderer->init(config);
    // // // hiZPassRenderer->init(config);
    // forwardRenderer->init(config);
	// // deferredRenderer->init(config);
	// raytracingRenderer->init(config);
    // // SSRGIPassRenderer->init(config);
    // // bloomRenderer->init(config);
    // // temporalPassRenderer->init(config);
    // // ddgiPassRenderer->init(config);
    // // deferredCombineRenderer->init(config);
	// // postProcessRenderer->init(config);

    for(auto& [name, renderer] : m_renderers) {
        renderer->init(config);
    }

    return true;
}

bool RendererManagerVulkan::onClose()
{
    Service::onClose();

    renderDeviceVulkan->waitIdle();
    for (auto& [name, renderer] : std::views::reverse(m_renderers)) {
        renderer->onClose();
    }
    // applicationRenderer->onClose();
	// forwardRenderer->onClose();
	// deferredRenderer->onClose();
	// raytracingRenderer->onClose();
	// shadowMapPass->onClose();
	// imageBasedRenderer->onClose();
    // alchemyAORenderer->onClose();
    // hiZPassRenderer->onClose();
    // SSRGIPassRenderer->onClose();
    // bloomRenderer->onClose();
    // temporalPassRenderer->onClose();
    // ddgiPassRenderer->onClose();
    // deferredCombineRenderer->onClose();
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
    Service::onUpdate();
    
    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    Camera& camera = *SceneManager::cameraController;

    if (firstFrame) {
        lastViewProj = camera.getProjectionMatrix() * camera.getViewMatrix();
        lastViewProj[1][1] *= -1.0; 
        firstFrame = false;
    }

    ubo.view = camera.getViewMatrix();
	ubo.prevViewProj = lastViewProj;
	ubo.proj = camera.getProjectionMatrix();
	ubo.cameraPos = glm::vec4(camera.getPosition(), 1.0);
	ubo.proj[1][1] *= -1.0;
	ubo.invView = camera.getInViewMatrix();
	ubo.invProj = camera.getInProjectionMatrix();
	ubo.invProj[1][1] *= -1.0;

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

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
        bloomRenderer->onUpdate();
        if(tmp->denoiserOn) {
            temporalPassRenderer->onUpdate();
        }
        ddgiPassRenderer->onUpdate();
        deferredCombineRenderer->onUpdate();
    } else if(currentRenderMode == 2) {
        raytracingRenderer->onUpdate();
    }

    particleRenderer->onUpdate();
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
        bloomRenderer->render(*camera);
        if(tmp->denoiserOn) {
            temporalPassRenderer->render(*camera);
        }
        ddgiPassRenderer->render(*camera);
        deferredCombineRenderer->render(*camera);
    } else if(currentRenderMode == 2) {
        raytracingRenderer->render(*camera);
    }

    particleRenderer->render(*camera);

    applicationRenderer->render(*camera);
    endFrame();
}


void RendererManagerVulkan::_cleanupResources()
{

}

void RendererManagerVulkan::_recreateResources()
{
	_cleanupResources();

	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	ubo.width = swapchain.swapChainExtent.width;
	ubo.height = swapchain.swapChainExtent.height;
}

RendererVulkan* RendererManagerVulkan::getRenderer(std::string_view name)
{
    for(auto& tuple : m_renderers) {
        if(name.data() == std::get<0>(tuple)) {
            return dynamic_cast<RendererVulkan*>(std::get<1>(tuple).get());
        }
    }
    return nullptr;
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

const RendererManagerVulkan::UniformBufferObject &RendererManagerVulkan::getUBO() const
{
    return ubo;
}

const std::vector<UniformBufferVulkan *> &RendererManagerVulkan::getBufferUBO() const
{
    return uniformbuffersList;
}
