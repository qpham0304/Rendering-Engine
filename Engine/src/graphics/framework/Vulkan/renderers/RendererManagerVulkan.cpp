#include "RendererManagerVulkan.h"
#include <core/scene/SceneManager.h>

#include "core/features/Timer.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ApplicationRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ForwardRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ParticleRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DebugRendererVulkan.h"
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
    
	m_eventSubscriptions.push_back(EventManager::getInstance().subscribe(EventType::WindowResize, [this] (Event& event) {
		this->needResize = true;
		this->swapchainOutdated = true;
	}));
    
	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	BufferManagerVulkan* bufferManagerVulkan = &dynamic_cast<BufferManagerVulkan&>(bufferManager);
    bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(Common::UniformBufferObject));


    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;
	ubo.width = swapchain.swapChainExtent.width;    //TODO: check if this update properly
	ubo.height = swapchain.swapChainExtent.height;

    const bool rtSupported = renderDeviceVulkan->device.raytracingSupport();
    if(!rtSupported) {
        m_logger->warn("Ray tracing is not supported on this gpu, ray tracing render mode and DDGI are disabled");
    }

    applicationRenderer = addRenderer<ApplicationRendererVulkan>("ApplicationRendererVulkan");

    shadowMapPass = addRenderer<ShadowMapPassVulkan>("ShadowMapPassVulkan");
    imageBasedRenderer = addRenderer<ImageBasedRendererVulkan>("ImageBasedRendererVulkan");

    forwardRenderer = addRenderer<ForwardRendererVulkan>("ForwardRendererVulkan");
    deferredRenderer = addRenderer<DeferredRendererVulkan>("DeferredRendererVulkan");
    if(rtSupported) {
        raytracingRenderer = addRenderer<RayTraceRendererVulkan>("RayTraceRendererVulkan");
    }

    alchemyAORenderer = addRenderer<AmbientOcclusionPassVulkan>("AmbientOcclusionPassVulkan");
    hiZPassRenderer = addRenderer<HiZPassVulkan>("HiZPassVulkan");
    SSRGIPassRenderer = addRenderer<SSRGIPassVulkan>("SSRGIPassVulkan");
    bloomRenderer = addRenderer<BloomPassVulkan>("BloomPassVulkan");
    temporalPassRenderer = addRenderer<TemporalPassVulkan>("TemporalPassVulkan");
    if(rtSupported) {
        ddgiPassRenderer = addRenderer<DDGIBuilderVulkan>("DDGIBuilderVulkan");
    }
    deferredCombineRenderer = addRenderer<DeferredCombinePassVulkan>("DeferredCombinePassVulkan");
    ////postProcessRenderer = addRenderer<PostProcessRendererVulkan>("postProcessRendererVulkan");
    particleRenderer = addRenderer<ParticleRendererVulkan>("ParticleRendererVulkan");
	debugRenderer = addRenderer<DebugRendererVulkan>("DebugRendererVulkan");

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
    return true;
}

void RendererManagerVulkan::destroy(uint32_t id)
{

}

std::vector<uint32_t> RendererManagerVulkan::listIDs() const
{
    return std::vector<uint32_t>();
}

void RendererManagerVulkan::onUpdate(const FrameTime& frameTime)
{
    Service::onUpdate(frameTime);
    static_cast<ParticleRendererVulkan*>(particleRenderer)->setSimDeltaTime(frameTime.gameDeltaTime);

    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

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
        if(ddgiPassRenderer) {
            ddgiPassRenderer->onUpdate();
        }
        deferredCombineRenderer->onUpdate();
    } else if(currentRenderMode == 2 && raytracingRenderer) {
        raytracingRenderer->onUpdate();
    }

    particleRenderer->onUpdate();
    debugRenderer->onUpdate();
}

void RendererManagerVulkan::render()
{
    Timer timer("Renderer Manager", true);
    Scene* scene = SceneManager::getInstance().getActiveScene();
    Camera* camera = SceneManager::cameraController;

	if(!SceneManager::cameraController) {
		return;
	}

    //make sure swapchain need to be resized first before other renderer can rebuild the new size
    if(swapchainOutdated) {
        renderDeviceVulkan->swapchain.recreateSwapchain();
        ubo.width = renderDeviceVulkan->swapchain.swapChainExtent.width;
        ubo.height = renderDeviceVulkan->swapchain.swapChainExtent.height;
        swapchainOutdated = false;
    }

    beginFrame();

    if (firstFrame) {
        lastViewProj = camera->getProjectionMatrix() * camera->getViewMatrix();
        lastViewProj[1][1] *= -1.0; 
        firstFrame = false;
    }

    ubo.view = camera->getViewMatrix();
	ubo.prevViewProj = lastViewProj;
	ubo.proj = camera->getProjectionMatrix();
	ubo.cameraPos = glm::vec4(camera->getPosition(), 1.0);
	ubo.proj[1][1] *= -1.0;
	ubo.invView = camera->getInViewMatrix();
	ubo.invProj = camera->getInProjectionMatrix();
	ubo.invProj[1][1] *= -1.0;

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));
	lastViewProj = ubo.proj * ubo.view;

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
        particleRenderer->render(*camera);
        SSRGIPassRenderer->render(*camera);
        bloomRenderer->render(*camera);
        if(tmp->denoiserOn) {
            temporalPassRenderer->render(*camera);
        }
        if(ddgiPassRenderer) {
            ddgiPassRenderer->render(*camera);
        }
        deferredCombineRenderer->render(*camera);
    } else if(currentRenderMode == 2 && raytracingRenderer) {
        raytracingRenderer->render(*camera);
    }

    debugRenderer->render(*camera);

    applicationRenderer->render(*camera);
    endFrame();
}

void RendererManagerVulkan::drawDebug(glm::vec3 start, glm::vec3 end)
{
    if(!debugRenderer) {
        m_logger->error("Could not request draw debug: Debug Renderer is not present");
        return;
    }

    auto* debugRendererVulkan = dynamic_cast<DebugRendererVulkan*>(debugRenderer);
    assert(debugRendererVulkan && "drawdebug: failed to cast debug renderer");

    debugRendererVulkan->drawLine(start, end);
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
    if(mode == 2 && !raytracingRenderer) {
        m_logger->warn("Ray tracing render mode is not available on this gpu");
        return;
    }
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

const Common::UniformBufferObject &RendererManagerVulkan::getUBO() const
{
    return ubo;
}

const std::vector<UniformBufferVulkan *> &RendererManagerVulkan::getBufferUBO() const
{
    return uniformbuffersList;
}
