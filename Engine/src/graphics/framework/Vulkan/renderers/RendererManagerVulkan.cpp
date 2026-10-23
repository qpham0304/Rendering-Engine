#include "RendererManagerVulkan.h"
#include <core/scene/SceneManager.h>
#include "core/features/Timer.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ApplicationRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ForwardRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ParticleRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/RibbonRendererVulkan.h"
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
    
    geometryPassRenderer = addRenderer<GeometryPassVulkan>("GeometryPassVulkan");
    deferredRenderer = addRenderer<DeferredRendererVulkan>("DeferredRendererVulkan");   //TODO: only ui atm, remove soon
    lightingPassRenderer = addRenderer<LightingPassVulkan>("LightingPassVulkan");
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
    particleRenderer = addRenderer<ParticleRendererVulkan>("ParticleRendererVulkan");
    ribbonRenderer = addRenderer<RibbonRendererVulkan>("RibbonRendererVulkan");
	debugRenderer = addRenderer<DebugRendererVulkan>("DebugRendererVulkan");

    // graph owned textures are sized from this, passes can create them during init
    VkExtent2D extent = renderDeviceVulkan->swapchain.swapChainExtent;
    m_renderGraph.setRenderSize(extent.width, extent.height);

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
    particleRenderer->setSimDeltaTime(frameTime.gameDeltaTime);
    particleRenderer->setRealDeltaTime(frameTime.deltaTime);   // the editor preview runs on it while the game is stopped
    ribbonRenderer->setDeltaTime(frameTime.deltaTime);

    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    if(currentRenderMode == 0) {
        forwardRenderer->onUpdate();
    } 
    else if(currentRenderMode == 1) {
        if(m_graphicsSettings.get<LightingSettings>()->ambientOcclusion) {
            alchemyAORenderer->onUpdate();
        }
        deferredRenderer->onUpdate();
        geometryPassRenderer->onUpdate();
        lightingPassRenderer->onUpdate();
        hiZPassRenderer->onUpdate();
        SSRGIPassRenderer->onUpdate();
        bloomRenderer->onUpdate();
        if(m_graphicsSettings.get<DeferredSettings>()->temporalReprojection) {
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
    ribbonRenderer->onUpdate();
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

    // graph owned textures get recreated on their next create when this changes
    VkExtent2D renderExtent = renderDeviceVulkan->swapchain.swapChainExtent;
    m_renderGraph.setRenderSize(renderExtent.width, renderExtent.height);

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

    VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();

    if(currentRenderMode == 0) {
        // forward is not on the graph yet, it still records the shared passes directly
        shadowMapPass->render(*camera);
        imageBasedRenderer->render(*camera);
        forwardRenderer->render(*camera);
    }
    else if(currentRenderMode == 1) {
        // m_renderGraph.compile();

        m_renderGraph.reset();

        // passes run in the order they are added
        shadowMapPass->addToGraph(m_renderGraph);
        imageBasedRenderer->addToGraph(m_renderGraph);
        geometryPassRenderer->addToGraph(m_renderGraph);

        if(m_graphicsSettings.get<LightingSettings>()->ambientOcclusion) {
            alchemyAORenderer->addToGraph(m_renderGraph);
        }
        
        lightingPassRenderer->addToGraph(m_renderGraph);
        hiZPassRenderer->addToGraph(m_renderGraph);
        particleRenderer->addToGraph(m_renderGraph);
        ribbonRenderer->addToGraph(m_renderGraph);
        SSRGIPassRenderer->addToGraph(m_renderGraph);
        bloomRenderer->addToGraph(m_renderGraph);
        if(m_graphicsSettings.get<DeferredSettings>()->temporalReprojection) {
            temporalPassRenderer->addToGraph(m_renderGraph);
        }
        if(ddgiPassRenderer) {
            ddgiPassRenderer->addToGraph(m_renderGraph);
        }
        deferredCombineRenderer->addToGraph(m_renderGraph);

        m_renderGraph.execute(cmd, currentFrame);
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

    debugRenderer->drawLine(start, end);
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
