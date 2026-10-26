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
#include "graphics/framework/vulkan/renderers/renderpasses/DepthPrepassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/AmbientOcclusionPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/SSRGIPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/BloomPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/TemporalPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/DeferredCombinePassVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/features/ImageBasedVulkan.h"
#include "graphics/framework/vulkan/renderers/features/SkyAtmosphereVulkan.h"
#include "graphics/framework/vulkan/renderers/features/DDGIBuilderVulkan.h"
#include "RenderDeviceVulkan.h"
#include "core/features/ServiceLocator.h"
#include "core/features/camera.h"
#include "core/events/EventManager.h"
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/features/Mesh.h>
#include <graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <filesystem>

RendererManagerVulkan::RendererManagerVulkan(std::string serviceName)
    : RendererManager(serviceName) 
{
    setFootprint("RendererManagerVulkan", sizeof(RendererManagerVulkan), MemorySystem::Graphics);
}

RendererManagerVulkan::~RendererManagerVulkan()
{
}

bool RendererManagerVulkan::init(WindowConfig config)
{
    Service::init(config);
	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);
	meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
	modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
    m_meshDraws.reserve(Common::MAX_INSTANCES);

	m_eventSubscriptions.push_back(EventManager::getInstance().subscribe(EventType::WindowResize, [this] (Event& event) {
		this->needResize = true;
		this->swapchainOutdated = true;
	}));
    
	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	bufferManagerVulkan = &dynamic_cast<BufferManagerVulkan&>(bufferManager);
    bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(Common::UniformBufferObject));

	MaterialManager& materialManager = ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
	materialManagerVulkan = &dynamic_cast<MaterialManagerVulkan&>(materialManager);

    // one copy per frame in flight, the cpu rewrites them every frame while the gpu may still read the previous one
    m_objects.resize(Common::MAX_INSTANCES);
    m_drawCommands.resize(Common::MAX_INSTANCES);
    for(uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
        uint32_t objectsID = bufferManagerVulkan->createBufferDeviceAddress(Common::MAX_INSTANCES * sizeof(Common::Object));
        m_objectsBufferIDs.push_back(objectsID);
        m_objectsAddresses.push_back(bufferManagerVulkan->getVirtualMemoryAddress(objectsID));

        m_drawCommandsBufferIDs.push_back(bufferManagerVulkan->createIndirectBuffer(Common::MAX_INSTANCES * sizeof(VkDrawIndirectCommand)));

        uint32_t lightsID = bufferManagerVulkan->createBufferDeviceAddress(Common::MAX_LIGHTS * sizeof(Common::PointLight));
        m_lightsBufferIDs.push_back(lightsID);
        m_lightsAddresses.push_back(bufferManagerVulkan->getVirtualMemoryAddress(lightsID));
    }


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
    skyAtmosphereRenderer = addRenderer<SkyAtmosphereVulkan>("SkyAtmosphereVulkan");
    imageBasedRenderer = addRenderer<ImageBasedRendererVulkan>("ImageBasedRendererVulkan");

    forwardRenderer = addRenderer<ForwardRendererVulkan>("ForwardRendererVulkan");
    
    depthPrepassRenderer = addRenderer<DepthPrepassVulkan>("DepthPrepassVulkan");
    geometryPassRenderer = addRenderer<GeometryPassVulkan>("GeometryPassVulkan");
    alchemyAORenderer = addRenderer<AmbientOcclusionPassVulkan>("AmbientOcclusionPassVulkan");
    deferredRenderer = addRenderer<DeferredRendererVulkan>("DeferredRendererVulkan");   //TODO: only ui atm, remove soon
    lightingPassRenderer = addRenderer<LightingPassVulkan>("LightingPassVulkan");
    if(rtSupported) {
        raytracingRenderer = addRenderer<RayTraceRendererVulkan>("RayTraceRendererVulkan");
    }

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

    // renderers loads saved values first before registering settings
    if(std::filesystem::exists(GraphicsSettingsSerializer::DefaultPath)) {
        m_graphicsSettings.loadSettings(GraphicsSettingsSerializer::DefaultPath);
    }

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
        depthPrepassRenderer->onUpdate();
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
    m_frameNumber++;

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

    collectMeshDraws();
    collectLights();
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
        skyAtmosphereRenderer->addToGraph(m_renderGraph);  // provides the environment the ibl builds from
        imageBasedRenderer->addToGraph(m_renderGraph);
        depthPrepassRenderer->addToGraph(m_renderGraph);

        if(m_graphicsSettings.get<LightingSettings>()->ambientOcclusion) {
            alchemyAORenderer->addToGraph(m_renderGraph);
        }

        geometryPassRenderer->addToGraph(m_renderGraph);

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

void RendererManagerVulkan::collectMeshDraws()
{
    m_prevModels.resize(m_meshDraws.size());
    for(const MeshDraw& draw : m_meshDraws) {
        m_prevModels[draw.objectIdx] = draw.model;
    }
    m_meshDraws.clear();

    Scene* scene = SceneManager::getInstance().getActiveScene();
    if(!scene) {
        return;
    }

    uint64_t materialsAddress = materialManagerVulkan->getMaterialAddress();
    scene->forEnitiesWith<TransformComponent, ModelComponent>([this, materialsAddress](Entity entity) {
        const Model* model = modelManager->getModel(entity.getComponent<ModelComponent>().modelID);
        if(!model) {
            return;
        }

        glm::mat4 worldMatrix = entity.getComponent<TransformComponent>().getModelMatrix();
        uint32_t boneMatricesIdx = entity.hasComponent<SkinningComponent>() ? entity.getComponent<SkinningComponent>().boneMatricesID : 0;
        const LightProbeComponent* probe = entity.hasComponent<LightProbeComponent>() ? &entity.getComponent<LightProbeComponent>() : nullptr;

        for(uint32_t i = 0; i < model->meshIDs.size(); i++) {
            if(m_meshDraws.size() >= Common::MAX_INSTANCES) {
                m_logger->warn("Scene has more than {} meshes, the rest are not drawn", Common::MAX_INSTANCES);
                return;
            }

            MeshDraw draw{};
            draw.model = probe ? glm::translate(worldMatrix, glm::vec3(probe->probeGrid[i])) : worldMatrix;	//TODO: this couple the probe data to the renderer
            draw.meshID = model->meshIDs[i];
            draw.objectIdx = static_cast<uint32_t>(m_meshDraws.size());
            m_meshDraws.push_back(draw);

            const Mesh* mesh = meshManager->getMesh(draw.meshID);
            const MeshManager::MeshData& meshData = meshManager->getMeshData(draw.meshID);

            Common::Object& object = m_objects[draw.objectIdx];
            object.vertexAddress = bufferManagerVulkan->getVirtualMemoryAddress(meshData.vertexBDA_ID);
            object.indexAddress = bufferManagerVulkan->getVirtualMemoryAddress(meshData.indexBDA_ID);
            object.materialsRef = materialsAddress;
            object.materialIndicesRef = bufferManagerVulkan->getVirtualMemoryAddress(meshData.matIndicesBDA_ID);
            object.model = draw.model;
            object.prevModel = draw.objectIdx < m_prevModels.size() ? m_prevModels[draw.objectIdx] : draw.model;
            object.boneMatricesIdx = boneMatricesIdx;
            object.materialIdx = mesh->materialID;
            object.flags = probe ? Common::OBJECT_LIGHT_PROBE : 0;

            VkDrawIndirectCommand& command = m_drawCommands[draw.objectIdx];
            command.vertexCount = static_cast<uint32_t>(mesh->indices.size());
            command.instanceCount = 1;
            command.firstVertex = 0;
            command.firstInstance = draw.objectIdx;
        }
    });

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    size_t count = m_meshDraws.size();
    bufferManagerVulkan->updateBufferDeviceAddress(m_objectsBufferIDs[currentFrame], m_objects.data(), count * sizeof(Common::Object));
    bufferManagerVulkan->getBuffer(m_drawCommandsBufferIDs[currentFrame])->update(m_drawCommands.data(), count * sizeof(VkDrawIndirectCommand));
}

// one indirect draw for every object, gl_InstanceIndex is the object index, shaders pull vertices through the objects table
void RendererManagerVulkan::drawObjects(VkCommandBuffer cmd)
{
    if(m_meshDraws.empty()) {
        return;
    }

    BufferVulkan* commands = bufferManagerVulkan->getBuffer(m_drawCommandsBufferIDs[renderDeviceVulkan->getCurrentFrameIndex()]);
    vkCmdDrawIndirect(cmd, static_cast<VkBuffer>(*commands), 0, static_cast<uint32_t>(m_meshDraws.size()), sizeof(VkDrawIndirectCommand));
}

// same values the light pass uploads, as a bda table for the traced passes (ddgi probes, ssr tlas hits)
void RendererManagerVulkan::collectLights()
{
    m_lights.clear();

    Scene* scene = SceneManager::getInstance().getActiveScene();
    if(!scene) {
        return;
    }

    scene->forEnitiesWith<TransformComponent, LightComponent>([this](Entity entity) {
        if(m_lights.size() >= Common::MAX_LIGHTS) {
            return;
        }

        auto& transform = entity.getComponent<TransformComponent>();
        auto& light = entity.getComponent<LightComponent>();

        Common::PointLight pointLight{};
        pointLight.color = light.color;
        pointLight.position = glm::vec4(glm::vec3(transform.getModelMatrix()[3]), 1.0f);
        pointLight.intensity = light.intensity * transform.scaleVec.x;
        m_lights.push_back(pointLight);
    });

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    bufferManagerVulkan->updateBufferDeviceAddress(m_lightsBufferIDs[currentFrame], m_lights.data(), m_lights.size() * sizeof(Common::PointLight));
}

uint64_t RendererManagerVulkan::getLightsAddress() const
{
    return m_lightsAddresses[renderDeviceVulkan->getCurrentFrameIndex()];
}

uint64_t RendererManagerVulkan::getObjectsAddress() const
{
    return m_objectsAddresses[renderDeviceVulkan->getCurrentFrameIndex()];
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
