#include "ParticleRendererVulkan.h"

#include <window/AppWindow.h>
#include <particle/ParticleManager.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/renderers/RenderDevice.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/scene/SceneManager.h>
#include <core/features/ServiceLocator.h>
#include <core/events/EventManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>

ParticleRendererVulkan::ParticleRendererVulkan(std::string serviceName)
	:	RendererVulkan(serviceName)
{

}

ParticleRendererVulkan::~ParticleRendererVulkan() 
{

}

bool ParticleRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

    emittersUBO.assign(1, {});   // need at least one emitter to upload, use dummy value at 0

    particleManager = &ServiceLocator::GetService<ParticleManager>("ParticleManager");
    geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

    containersRefID = bufferManagerVulkan->createBufferDeviceAddress(MAX_CONTAINERS * sizeof(ContainerRefs));

    _uploadContainerRefs();
    
    for(uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
        emittersUBORefIDs.push_back(bufferManagerVulkan->createBufferDeviceAddress(MAX_EMITTERS * sizeof(EmitterUBO)));
        _uploadEmitters(i);
    }

    pushConstant.containersRef = bufferManagerVulkan->getBuffer(containersRefID)->getAddress();
    pushConstant.emitterRef = bufferManagerVulkan->getBuffer(emittersUBORefIDs[0])->getAddress();

    _createDescriptor();
    _createPipelines();

    return true;
}

bool ParticleRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();

    return true;
}

void ParticleRendererVulkan::onUpdate()
{
    const auto& containers = particleManager->getAllContainerData();
    size_t containerCount = std::min<size_t>(containers.size(), MAX_CONTAINERS);

    //recreate the entire bda reference table to match the particleManager's container data
    if(containerRefs.size() != containerCount) {
        containerRefs.clear();
        containerRefs.push_back({});    // again, location 0 for default value and debug
        renderDeviceVulkan->waitIdle();
        emittersUBO.assign(1, {});
        for(size_t i = 1; i < containerCount; i++) {
            const auto& container = containers[i];
            ContainerRefs refs{};
            refs.lifetimeBufferRef = bufferManagerVulkan->getBuffer(container.lifetimeBufferID)->getAddress();
            refs.positionsBufferRef = bufferManagerVulkan->getBuffer(container.positionsBufferID)->getAddress();
            refs.scalesBufferRef = bufferManagerVulkan->getBuffer(container.scalesBufferID)->getAddress();
            refs.velocitiesBufferRef = bufferManagerVulkan->getBuffer(container.velocitiesBufferID)->getAddress();
            refs.colorsBufferRef = bufferManagerVulkan->getBuffer(container.colorsBufferID)->getAddress();
            containerRefs.push_back(refs);
            emittersUBO.push_back({});
        }
        _uploadContainerRefs();
    }

    if(containerRefs.size() <= 1) {
        return;
    }

    SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
		return;
	}


    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        TransformComponent& transform = entity.getComponent<TransformComponent>();
        ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
        if(!_isValidContainer(emitter.containerID)) {
            return;
        }

        emitter.spawnPosition = transform.translateVec;
        EmitterUBO& emitterUBO = emittersUBO[emitter.containerID];

        emitterUBO.emitMax = emitter.emitMax;
        emitterUBO.emitCount = emitter.emitCount;
        emitterUBO.areRecycled = emitter.areRecycled;
        emitterUBO.emitAccumulator = emitter.emitAccumulator;
        emitterUBO.emitRate = emitter.emitRate;
        emitterUBO.lifetimeMin = emitter.lifetimeMin;
        emitterUBO.lifetimeMax = emitter.lifetimeMax;
        emitterUBO.speedMin = emitter.speedMin;
        emitterUBO.speedMax = emitter.speedMax;
        emitterUBO.spawnPosition = emitter.spawnPosition;   // TODO: might not enforce this to transform and let user decide
        emitterUBO.force = emitter.force;
        emitterUBO.resetPosition = emitter.resetPosition;
        emitterUBO.behaviorType = emitter.behaviorType;

        if(entity.hasComponent<SpriteComponent>()) {
            SpriteComponent& sprite = entity.getComponent<SpriteComponent>();
            emitterUBO.textureID = sprite.textureID;
            emitterUBO.uvScale = sprite.uvScale;
            emitterUBO.uvOffset = sprite.uvOffset;
        }

    });

    scene->forEnitiesWith<ParticleEmitter>(func);

}

void ParticleRendererVulkan::render(Camera &camera)
{
    Timer timer(m_name, true);

    if(containerRefs.size() <= 1) {
        return;
    }

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
		return;
	}

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();

    _uploadEmitters(currentFrame);
    pushConstant.emitterRef = bufferManagerVulkan->getBuffer(emittersUBORefIDs[currentFrame])->getAddress();

    pushConstant.deltaTime = std::min(simDeltaTime, MAX_SIM_STEP);  // 0 while paused or stopped

    renderDeviceVulkan->beginLabel(cmd, "Particle Compute Pass", {1.0, 1.0, 0.5, 1.0});
    _computeParticle(cmd, currentFrame, scene);
    renderDeviceVulkan->endLabel(cmd);
    
    renderDeviceVulkan->beginLabel(cmd, "Particle Render Pass", {1.0, 0.0, 1.0, 1.0});
    _renderParticle(cmd, currentFrame, scene);
	renderDeviceVulkan->endLabel(cmd);
    
}

void ParticleRendererVulkan::_recreateResources()
{
}

void ParticleRendererVulkan::_cleanupResources()
{
    pipeline->destroy();
    computePipeline->destroy();
}

void ParticleRendererVulkan::_createPipelines()
{
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	PipelineConfigInfo config = VulkanPipeline::defaultPipelineConfigInfo(1);
	config.renderPass = VK_NULL_HANDLE;
    config.setAdditiveBlend();
   
    config.depthStencilInfo.depthTestEnable = VK_TRUE;   // depth test against the scene depth but never write it
    config.depthStencilInfo.depthWriteEnable = VK_FALSE; // additive blending does not need sorting

	AttachmentsInfo attachmentsInfo{};
    attachmentsInfo.colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };   // deferred combine output format
    attachmentsInfo.depthAttachmentFormat = depthFormat;
    attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;


    VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
	VkDescriptorPool descriptorPool = descriptorManagerVulkan->getDescriptorPool(poolID);

	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

	// void* handle = materialManager->getMaterialLayout();
	// auto materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

    pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createGraphicsPipelineDynamic(
        "assets/shaders/spv/particle.vert.spv",
        "assets/shaders/spv/particle.frag.spv",
        config,
        attachmentsInfo,
        vertexInputInfo,
        { descriptorSetLayout, bindlessLayout },
        sizeof(pushConstant)
    );

    computePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    computePipeline->createComputePipeline(
        "assets/shaders/spv/particle.comp.spv",
        { descriptorSetLayout, bindlessLayout },
        sizeof(pushConstant)
    );
}

void ParticleRendererVulkan::_createDescriptor()
{
    layoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
	};
	
	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);

    _updateDescriptor();

}

void ParticleRendererVulkan::_updateDescriptor()
{
	descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

    auto uniformBuffersList = rendererManagerVulkan->getBufferUBO();
	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		DescriptorWriter writer{{}, descriptorSets[i] };
		descriptorManagerVulkan->writeUniform2(writer, uniformBuffersList[i]->getDescUniformBufferInfo());
		descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
	}
}

void ParticleRendererVulkan::_computeParticle(VkCommandBuffer cmd, uint32_t currentFrame, Scene* scene)
{
    // previous sim step and last frame's draw must be done before this step overwrites the attributes
    VkMemoryBarrier preSim{};
    preSim.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    preSim.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    preSim.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &preSim, 0, nullptr, 0, nullptr
    );

    computePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
        if(!_isValidContainer(emitter.containerID)) {
            return;
        }
        ParticleContainer container = particleManager->getContainer(emitter.containerID);
        ParticleManager::ContainerData containerData = particleManager->getContainerData(emitter.containerID);
        if(container.m_size == 0) {
            return;
        }

        pushConstant.containerIdx = emitter.containerID;
        pushConstant.particleCount = container.m_size;
        vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlePushConstant), &pushConstant);

        uint32_t groupX_size = 256;
        vkCmdDispatch(cmd, (container.m_size + groupX_size - 1) / groupX_size, 1, 1);
    });

    scene->forEnitiesWith<ParticleEmitter>(func);

    // the draw reads the simulated attributes in the vertex and fragment shaders
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 1, &memoryBarrier, 0, nullptr, 0, nullptr
    );
}

void ParticleRendererVulkan::_renderParticle(VkCommandBuffer cmd, uint32_t currentFrame, Scene* scene)
{
    if(!geometryPass || !lightingPass) {
        return;
    }
    TextureVulkan* sceneImage = lightingPass->renderTarget.colorTexture;
    TextureVulkan* depthImage = geometryPass->renderTarget.depthTexture;
    uint32_t width = geometryPass->renderTarget.width;
    uint32_t height = geometryPass->renderTarget.height;

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = sceneImage->textureImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthImage->textureImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {width, height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];

    vkCmdBeginRendering(cmd, &renderingInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 1, 1, &bindlessSet, 0, nullptr);

    renderDeviceVulkan->setViewport(width, height);
    renderDeviceVulkan->setScissor(width, height);

    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
        if(!_isValidContainer(emitter.containerID)) {
            return;
        }
        ParticleContainer container = particleManager->getContainer(emitter.containerID);
        if(container.m_size == 0) {
            return;
        }

        pushConstant.containerIdx = emitter.containerID;
        pushConstant.particleCount = container.m_size;
        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ParticlePushConstant), &pushConstant);

        vkCmdDraw(cmd, container.m_size * 6, 1, 0, 0);  //TODO: use drawindirect with indirect buffer for efficiency ignore for now
    });

    scene->forEnitiesWith<ParticleEmitter>(func);

    vkCmdEndRendering(cmd);
}


void ParticleRendererVulkan::_uploadContainerRefs()
{
    size_t count = std::min<size_t>(containerRefs.size(), MAX_CONTAINERS);
    bufferManagerVulkan->updateBufferDeviceAddress(containersRefID, containerRefs.data(), count * sizeof(ContainerRefs));
}

void ParticleRendererVulkan::_uploadEmitters(uint32_t frame)
{
    size_t count = std::min<size_t>(emittersUBO.size(), MAX_EMITTERS);
    bufferManagerVulkan->updateBufferDeviceAddress(emittersUBORefIDs[frame], emittersUBO.data(), count * sizeof(EmitterUBO));
}

bool ParticleRendererVulkan::_isValidContainer(uint32_t containerID) const
{
    return containerID != 0 && containerID < containerRefs.size() && containerID < emittersUBO.size();
}

void ParticleRendererVulkan::addToGraph(RenderGraphVulkan& graph)
{
    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    TextureVulkan* sceneImage = lightingPass->renderTarget.colorTexture;
    TextureVulkan* depthImage = geometryPass->renderTarget.depthTexture;

    auto scene = graph.importTexture(sceneImage);
    auto depth = graph.importTexture(depthImage);
    
    RenderPassNode node{};
    node.name = "Particle";
    node.reads  = {
        { depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL } 
    };

    node.writes = { 
        { scene, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } 
    };

    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        Scene* scene = SceneManager::getInstance().getActiveScene();
        Camera* camera = SceneManager::cameraController;

        if(!camera) {
            return;
        }

        render(*camera);
    };

    graph.addPass(node);
}
