#include "ParticleRendererVulkan.h"

#include <window/AppWindow.h>
#include <particle/ParticleManager.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/BufferVulkan.h>
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
#include <core/components/MComponent.h>
#include <graphics/framework/Vulkan/core/CommonStructs.h>

static_assert(static_cast<size_t>(ParticleBlendMode::Count) == 2, "one particle pipeline per blend mode and cull mode");

ParticleRendererVulkan::ParticleRendererVulkan(std::string serviceName)
	:	RendererVulkan(serviceName)
{
    setFootprint("ParticleRendererVulkan", sizeof(ParticleRendererVulkan), MemorySystem::Graphics);

}

ParticleRendererVulkan::~ParticleRendererVulkan()
{

}

bool ParticleRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

    particleManager = &ServiceLocator::GetService<ParticleManager>("ParticleManager");
    geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

    containersRefID = bufferManagerVulkan->createBufferDeviceAddress(MAX_CONTAINERS * sizeof(ContainerRefs));
    _uploadContainerRefs();

    emittersGPU.assign(MAX_CONTAINERS, {});     // slot 0 stays a dummy like the containers
    for(uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
        emittersRefIDs.push_back(bufferManagerVulkan->createBufferDeviceAddress(MAX_CONTAINERS * sizeof(EmitterGPU)));
        _uploadEmitters(i);
    }

    // device local, only touched by vkCmdUpdateBuffer, the simulate pass and the indirect draw
    drawCommands.assign(MAX_CONTAINERS, {});
    drawCommandsID = bufferManagerVulkan->createBuffer2(
        MAX_CONTAINERS * sizeof(VkDrawIndirectCommand),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    pushConstant.containersRef = bufferManagerVulkan->getBuffer(containersRefID)->getAddress();
    pushConstant.emitterRef = bufferManagerVulkan->getBuffer(emittersRefIDs[0])->getAddress();
    pushConstant.drawCommandsRef = bufferManagerVulkan->getBuffer(drawCommandsID)->getAddress();

    // the preview camera gets its own copy of the frame UBO so the same shaders draw it unchanged
    bufferManagerVulkan->createUniformBuffers(previewUBOs, sizeof(Common::UniformBufferObject));

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
    SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
		return;
	}

    // anything that rewrites gpu buffers or loads assets happens here, outside of command recording
    bool deviceIdle = false;
    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
        _prepareEmitter(emitter, deviceIdle);
        _resolveAssets(emitter);
    });

    scene->forEnitiesWith<ParticleEmitter>(func);

    // in flight frames still read the old address table
    if(containersVersion != particleManager->getVersion()) {
        if(!deviceIdle) {
            renderDeviceVulkan->waitIdle();
        }
        _rebuildContainerRefs();
    }
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

    float gameStep = std::min(simDeltaTime, MAX_SIM_STEP);  // 0 while paused or stopped
    float previewStep = (previewActive && preview.simulate) ? std::min(realDeltaTime * preview.timeScale, MAX_SIM_STEP) : 0.0f;
    simTime = std::fmod(simTime + std::max(gameStep, previewStep), 3600.0f);
    frameSeed++;

    _gatherEmitters(scene, gameStep, previewStep);

    if(activeEmitters.empty()) {
        return;
    }

    _uploadEmitters(currentFrame);
    if(previewActive) {
        _uploadPreviewCamera(currentFrame);
    }
    pushConstant.emitterRef = bufferManagerVulkan->getBuffer(emittersRefIDs[currentFrame])->getAddress();
    pushConstant.time = simTime;
    pushConstant.frameSeed = frameSeed;

    renderDeviceVulkan->beginLabel(cmd, "Particle Compute Pass", {1.0, 1.0, 0.5, 1.0});
    _computeParticle(cmd, currentFrame);
    renderDeviceVulkan->endLabel(cmd);

    renderDeviceVulkan->beginLabel(cmd, "Particle Render Pass", {1.0, 0.0, 1.0, 1.0});
    _renderParticle(cmd, currentFrame);
	renderDeviceVulkan->endLabel(cmd);

}

void ParticleRendererVulkan::_recreateResources()
{
}

void ParticleRendererVulkan::_cleanupResources()
{
    for(auto& pipeline : pipelines) {
        pipeline->destroy();
    }
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

	AttachmentsInfo attachmentsInfo{};
    attachmentsInfo.colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };   // deferred combine output format
    attachmentsInfo.depthAttachmentFormat = depthFormat;
    attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);

	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

    // piplines for blend mode * 2 + back face culling
    for(size_t i = 0; i < pipelines.size(); i++) {
        int blendMode = static_cast<int>(i / 2);
        bool cullBackFaces = (i % 2) == 1;

        PipelineConfigInfo config = VulkanPipeline::defaultPipelineConfigInfo(1);
        config.renderPass = VK_NULL_HANDLE;
        if(blendMode == static_cast<int>(ParticleBlendMode::Alpha)) {
            config.setAlphaBlend();     // the draw list is sorted back to front for these
        } else {
            config.setAdditiveBlend();
        }

        // the shared presets write the particle's own alpha into the target, so the result depends on which particle
        // drew last, additive is never sorted and that changes every frame, which shows wherever the alpha is used
        // (the editor preview is drawn through imgui with blending), keep the target alpha instead
        for(auto& attachment : config.colorBlendAttachments) {
            if(blendMode == static_cast<int>(ParticleBlendMode::Alpha)) {
                attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;                    // standard over, stays 1 on an opaque target
                attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            } else {
                attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;                   // light only adds color
                attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            }
        }

        // quads stay double sided, closed meshes cull their back faces so the far side never blends over the near side
        config.rasterizationInfo.cullMode = cullBackFaces ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        config.depthStencilInfo.depthTestEnable = VK_TRUE;      // depth test against the scene depth but never write it
        config.depthStencilInfo.depthWriteEnable = VK_FALSE;

        pipelines[i] = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
        pipelines[i]->createGraphicsPipelineDynamic(
            "assets/shaders/spv/particle.vert.spv",
            "assets/shaders/spv/particle.frag.spv",
            config,
            attachmentsInfo,
            vertexInputInfo,
            { descriptorSetLayout, bindlessLayout },
            sizeof(pushConstant)
        );
    }

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

	// one set per frame for the main camera and one for the preview camera
	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 2},
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount * 2, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);
	previewSetsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);

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

	previewDescriptorSets = descriptorManagerVulkan->getDescriptorSet(previewSetsID);
	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		DescriptorWriter writer{{}, previewDescriptorSets[i] };
		descriptorManagerVulkan->writeUniform2(writer, previewUBOs[i]->getDescUniformBufferInfo());
		descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
	}
}

void ParticleRendererVulkan::_prepareEmitter(ParticleEmitter& emitter, bool& deviceIdle)
{
    auto waitIdle = [&]() {
        if(!deviceIdle) {
            renderDeviceVulkan->waitIdle();
            deviceIdle = true;
        }
    };

    emitter.maxParticles = std::clamp(emitter.maxParticles, 1, MAX_PARTICLES_PER_CONTAINER);
    uint32_t capacity = static_cast<uint32_t>(emitter.maxParticles);

    bool restarted = false;
    if(emitter.containerID == 0) {
        emitter.containerID = particleManager->createContainer(capacity);
        restarted = true;
    } else if(particleManager->getContainer(emitter.containerID).m_size != capacity) {
        waitIdle();
        particleManager->resizeContainer(emitter.containerID, capacity);
        restarted = true;
    } else if(emitter.resetRequested) {
        waitIdle();
        particleManager->resetContainer(emitter.containerID);
        restarted = true;
    }

    if(restarted) {
        emitter.emitAccumulator = 0.0f;
        emitter.burstTimer = 0.0f;
    }
    emitter.resetRequested = false;
}

void ParticleRendererVulkan::_resolveAssets(ParticleEmitter& emitter)
{
    // "None" keeps whatever textureID was assigned directly
    if(emitter.texturePath != emitter.loadedTexturePath) {
        emitter.loadedTexturePath = emitter.texturePath;
        if(emitter.texturePath != "None" && !emitter.texturePath.empty()) {
            emitter.textureID = textureManagerVulkan->loadTexture(emitter.texturePath, 1, false);
        }
    }

    // loading a texture does not put it in the bindless array, an unwritten slot samples as transparent black
    if(emitter.textureID != 0 && !bindlessTextures.contains(emitter.textureID)) {
        textureManagerVulkan->registerBindlessTexture(emitter.textureID);
        bindlessTextures.insert(emitter.textureID);
    }

    // meshes only load once an emitter actually draws them
    if(emitter.renderMode != static_cast<int>(ParticleRenderMode::Mesh)) {
        return;
    }

    if(emitter.meshPath != emitter.loadedMeshPath) {
        emitter.loadedMeshPath = emitter.meshPath;
        emitter.meshModelID = 0;
        if(emitter.meshPath != "None" && !emitter.meshPath.empty()) {
            emitter.meshModelID = modelManager->loadModel(emitter.meshPath);
        }
        if(emitter.meshModelID == 0) {
            m_logger->warn("particle mesh '{}' failed to load, falling back to billboards", emitter.meshPath);
        }
    }

    if(emitter.meshModelID == 0 || meshRefs.contains(emitter.meshModelID)) {
        return;
    }

    const Model* model = modelManager->getModel(emitter.meshModelID);
    if(!model || model->meshIDs.empty()) {
        return;
    }

    //TODO: only the first mesh of a model is drawn, multi mesh models need one draw per mesh
    uint32_t meshID = model->meshIDs[0];
    const Mesh* mesh = meshManager->getMesh(meshID);
    MeshManager::MeshData meshData = meshManager->getMeshData(meshID);
    if(!mesh || meshData.vertexBDA_ID == 0 || meshData.indexBDA_ID == 0) {
        return;
    }

    MeshRefs refs{};
    refs.verticesRef = bufferManagerVulkan->getBuffer(meshData.vertexBDA_ID)->getAddress();
    refs.indicesRef = bufferManagerVulkan->getBuffer(meshData.indexBDA_ID)->getAddress();
    refs.indexCount = static_cast<uint32_t>(mesh->indices.size());

    // the material already registered its textures in the bindless array
    auto* materials = dynamic_cast<MaterialManagerVulkan*>(materialManager);
    if(materials && mesh->materialID != 0) {
        MaterialDesc material = materials->getMaterial(mesh->materialID);
        if(!material.albedoIDs.empty() && material.albedoIDs[0] != materials->getFallbackAlbedoID()) {
            refs.albedoTextureID = material.albedoIDs[0];
        }
    }
    meshRefs[emitter.meshModelID] = refs;
}

void ParticleRendererVulkan::_rebuildContainerRefs()
{
    const auto& containers = particleManager->getAllContainerData();
    size_t containerCount = std::min<size_t>(containers.size(), MAX_CONTAINERS);

    // the address table, emitter array and draw commands only have MAX_CONTAINERS slots, slot 0 is the dummy
    if(containers.size() > MAX_CONTAINERS) {
        m_logger->warn("{} particle containers exist but the renderer only draws ids 1 to {}, emitters past that will not be drawn",
            containers.size() - 1, MAX_CONTAINERS - 1);
    }

    containerRefs.assign(1, {});    // location 0 for default value and debug
    for(size_t i = 1; i < containerCount; i++) {
        const auto& container = containers[i];
        ContainerRefs refs{};
        if(container.particlesBufferID != 0) {     // destroyed slots stay zero
            refs.particlesRef = bufferManagerVulkan->getBuffer(container.particlesBufferID)->getAddress();
            refs.aliveListRef = bufferManagerVulkan->getBuffer(container.aliveListBufferID)->getAddress();
            refs.deadListRef = bufferManagerVulkan->getBuffer(container.deadListBufferID)->getAddress();
            refs.countersRef = bufferManagerVulkan->getBuffer(container.countersBufferID)->getAddress();
        }
        containerRefs.push_back(refs);
    }

    _uploadContainerRefs();
    containersVersion = particleManager->getVersion();
}

void ParticleRendererVulkan::_gatherEmitters(Scene* scene, float gameStep, float previewStep)
{
    activeEmitters.clear();
    std::fill(drawCommands.begin(), drawCommands.end(), VkDrawIndirectCommand{});

    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
        if(!_isValidContainer(emitter.containerID)) {
            return;
        }
        uint32_t capacity = particleManager->getContainer(emitter.containerID).m_size;
        if(capacity == 0) {
            return;
        }

        // while the game is stopped the editor can run its emitter alone, a playing game drives everything
        float deltaTime = gameStep;
        if(gameStep == 0.0f && _isPreviewed(emitter.containerID)) {
            deltaTime = previewStep;
        }

        // spawn budget for this frame, fractional particles carry over to the next one
        uint32_t spawnCount = 0;
        if(emitter.isPlaying && deltaTime > 0.0f) {
            emitter.emitAccumulator += std::max(emitter.emitRate, 0.0f) * deltaTime;
            spawnCount = static_cast<uint32_t>(std::min(emitter.emitAccumulator, static_cast<float>(capacity)));
            emitter.emitAccumulator = std::min(emitter.emitAccumulator - spawnCount, 1.0f);

            if(emitter.burstCount > 0 && emitter.burstInterval > 0.0f) {
                emitter.burstTimer -= deltaTime;
                if(emitter.burstTimer <= 0.0f) {
                    spawnCount += emitter.burstCount;
                    emitter.burstTimer = emitter.burstInterval;
                }
            }
        }
        spawnCount += static_cast<uint32_t>(std::max(emitter.pendingBurst, 0));
        emitter.pendingBurst = 0;
        spawnCount = std::min(spawnCount, capacity);

        TransformComponent& transform = entity.getComponent<TransformComponent>();
        EmitterGPU& gpu = emittersGPU[emitter.containerID];

        gpu.transform = transform.getModelMatrix();
        gpu.colorStart = emitter.colorStart;
        gpu.colorEnd = emitter.colorEnd;
        gpu.shapeExtents = emitter.shapeExtents;
        gpu.shapeRadius = emitter.shapeRadius;
        gpu.direction = emitter.direction;
        gpu.spreadAngle = glm::radians(emitter.spreadAngle);
        gpu.force = emitter.force;
        gpu.gravityScale = emitter.gravityScale;
        gpu.shapeConeAngle = glm::radians(emitter.shapeConeAngle);
        gpu.speedMin = emitter.speedMin;
        gpu.speedMax = emitter.speedMax;
        gpu.lifetimeMin = emitter.lifetimeMin;
        gpu.lifetimeMax = emitter.lifetimeMax;
        gpu.drag = emitter.drag;
        gpu.turbulenceStrength = emitter.turbulenceStrength;
        gpu.turbulenceFrequency = emitter.turbulenceFrequency;
        gpu.vortexStrength = emitter.vortexStrength;
        gpu.attractStrength = emitter.attractStrength;
        gpu.sizeMin = emitter.sizeMin;
        gpu.sizeMax = emitter.sizeMax;
        gpu.sizeEndScale = emitter.sizeEndScale;
        gpu.rotationMin = glm::radians(emitter.rotationMin);
        gpu.rotationMax = glm::radians(emitter.rotationMax);
        gpu.angularVelocityMin = glm::radians(emitter.angularVelocityMin);
        gpu.angularVelocityMax = glm::radians(emitter.angularVelocityMax);
        gpu.fadeIn = emitter.fadeIn;
        gpu.fadeOut = emitter.fadeOut;
        gpu.stretchFactor = emitter.stretchFactor;
        gpu.flipbookFPS = emitter.flipbookFPS;
        gpu.meshShading = emitter.meshShading;
        gpu.spawnShape = emitter.spawnShape;
        gpu.velocityFromShape = emitter.velocityFromShape ? 1 : 0;
        gpu.localSpace = emitter.localSpace ? 1 : 0;
        gpu.renderMode = emitter.renderMode;
        gpu.flipbookRows = emitter.flipbookRows;
        gpu.flipbookCols = emitter.flipbookCols;
        gpu.emitFromShell = emitter.emitFromShell ? 1 : 0;

        uint32_t vertexCount = QUAD_VERTEX_COUNT;
        gpu.meshVerticesRef = 0;
        gpu.meshIndicesRef = 0;
        const MeshRefs* mesh = nullptr;
        if(emitter.renderMode == static_cast<int>(ParticleRenderMode::Mesh)) {
            auto it = meshRefs.find(emitter.meshModelID);
            if(it != meshRefs.end() && it->second.indexCount > 0) {
                mesh = &it->second;
                gpu.meshVerticesRef = mesh->verticesRef;
                gpu.meshIndicesRef = mesh->indicesRef;
                vertexCount = mesh->indexCount;
            } else {
                gpu.renderMode = static_cast<int>(ParticleRenderMode::Billboard);
            }
        }

        // an explicit texture wins, then a sprite on the same entity, then the mesh's own albedo
        uint32_t textureID = 0;
        ParticleTextureSource source = ParticleTextureSource::None;
        if(emitter.textureID != 0) {
            textureID = emitter.textureID;
            source = ParticleTextureSource::Emitter;
        } else if(entity.hasComponent<SpriteComponent>() && entity.getComponent<SpriteComponent>().textureID != 0) {
            textureID = entity.getComponent<SpriteComponent>().textureID;
            source = ParticleTextureSource::Sprite;
        } else if(mesh && emitter.useModelTexture && mesh->albedoTextureID != 0) {
            textureID = mesh->albedoTextureID;
            source = ParticleTextureSource::Model;
        }
        gpu.textureID = static_cast<int>(textureID);
        emitter.activeTextureID = textureID;
        emitter.activeTextureSource = static_cast<int>(source);

        drawCommands[emitter.containerID].vertexCount = vertexCount;

        ActiveEmitter active{};
        active.containerIdx = emitter.containerID;
        active.capacity = capacity;
        active.spawnCount = spawnCount;
        active.deltaTime = deltaTime;
        active.blendMode = std::clamp(emitter.blendMode, 0, static_cast<int>(ParticleBlendMode::Count) - 1);
        active.cullBackFaces = gpu.renderMode == static_cast<int>(ParticleRenderMode::Mesh) && !emitter.meshDoubleSided;
        if(active.blendMode == static_cast<int>(ParticleBlendMode::Alpha)) {
            active.sortSize = particleSortSize(capacity);   // additive blending is order independent
        }
        activeEmitters.push_back(active);
    });

    scene->forEnitiesWith<ParticleEmitter>(func);

    // alpha blended emitters first, additive is order independent and goes on top
    std::stable_sort(activeEmitters.begin(), activeEmitters.end(), [](const ActiveEmitter& a, const ActiveEmitter& b) {
        return a.blendMode > b.blendMode;
    });
}

void ParticleRendererVulkan::_computeParticle(VkCommandBuffer cmd, uint32_t currentFrame)
{
    VkBuffer drawBuffer = static_cast<VkBuffer>(*bufferManagerVulkan->getBuffer(drawCommandsID));

    // last frame's simulate and draw must be done with the particles and indirect commands before they are rewritten
    VkMemoryBarrier preSim{};
    preSim.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    preSim.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    preSim.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &preSim, 0, nullptr, 0, nullptr
    );

    // vertex counts come from the cpu, instance counts restart at 0 and are rebuilt by the simulate pass
    vkCmdUpdateBuffer(cmd, drawBuffer, 0, drawCommands.size() * sizeof(VkDrawIndirectCommand), drawCommands.data());

    VkMemoryBarrier commandsReset{};
    commandsReset.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    commandsReset.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    commandsReset.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &commandsReset, 0, nullptr, 0, nullptr
    );

    computePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    // emit: pop free slots and initialize the new particles
    pushConstant.stage = STAGE_EMIT;
    for(const ActiveEmitter& active : activeEmitters) {
        if(active.spawnCount == 0) {
            continue;
        }
        pushConstant.containerIdx = active.containerIdx;
        pushConstant.particleCount = active.spawnCount;
        pushConstant.deltaTime = active.deltaTime;
        vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlePushConstant), &pushConstant);
        vkCmdDispatch(cmd, (active.spawnCount + GROUP_SIZE - 1) / GROUP_SIZE, 1, 1);
    }

    // simulate reads the slots emit wrote and the dead count it left
    VkMemoryBarrier emitToSim{};
    emitToSim.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    emitToSim.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    emitToSim.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &emitToSim, 0, nullptr, 0, nullptr
    );

    // simulate: age, integrate, free the dead and append the survivors to the draw list
    // the sort keys come from the camera in set 0, the previewed emitter is keyed for the preview camera
    pushConstant.stage = STAGE_SIMULATE;
    for(const ActiveEmitter& active : activeEmitters) {
        bool previewed = _isPreviewed(active.containerIdx);
        if(previewed) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &previewDescriptorSets[currentFrame], 0, nullptr);
        }

        pushConstant.containerIdx = active.containerIdx;
        pushConstant.particleCount = active.capacity;
        pushConstant.deltaTime = active.deltaTime;
        vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlePushConstant), &pushConstant);
        vkCmdDispatch(cmd, (active.capacity + GROUP_SIZE - 1) / GROUP_SIZE, 1, 1);

        if(previewed) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
        }
    }

    _sortParticles(cmd);

    // the draw consumes the instance counts as indirect args and the particles in the vertex shader
    VkMemoryBarrier simToDraw{};
    simToDraw.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    simToDraw.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    simToDraw.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 1, &simToDraw, 0, nullptr, 0, nullptr
    );
}

void ParticleRendererVulkan::_sortParticles(VkCommandBuffer cmd)
{
    uint32_t maxSortSize = 0;
    for(const ActiveEmitter& active : activeEmitters) {
        maxSortSize = std::max(maxSortSize, active.sortSize);
    }
    if(maxSortSize < 2) {
        return;
    }

    VkMemoryBarrier computeToCompute{};
    computeToCompute.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    computeToCompute.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    computeToCompute.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    auto barrier = [&]() {
        vkCmdPipelineBarrier(
            cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &computeToCompute, 0, nullptr, 0, nullptr
        );
    };

    // every step dispatches all sorted emitters at once so they share one barrier per step
    auto dispatchStep = [&](uint32_t stage, uint32_t block, uint32_t stride) {
        pushConstant.stage = stage;
        pushConstant.sortBlock = block;
        pushConstant.sortStride = stride;
        for(const ActiveEmitter& active : activeEmitters) {
            if(active.sortSize < 2 || block > active.sortSize) {
                continue;
            }
            pushConstant.containerIdx = active.containerIdx;
            pushConstant.particleCount = active.sortSize;
            vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlePushConstant), &pushConstant);
            vkCmdDispatch(cmd, (active.sortSize + GROUP_SIZE - 1) / GROUP_SIZE, 1, 1);
        }
        barrier();
    };

    // simulate has to finish writing the list and its live count first
    barrier();
    dispatchStep(STAGE_SORT_PAD, 0, 0);

    //TODO: steps with a stride under the group size could run in one dispatch through shared memory
    for(uint32_t block = 2; block <= maxSortSize; block <<= 1) {
        for(uint32_t stride = block >> 1; stride > 0; stride >>= 1) {
            dispatchStep(STAGE_SORT, block, stride);
        }
    }
}

VulkanPipeline* ParticleRendererVulkan::_getPipeline(int blendMode, bool cullBackFaces) const
{
    return pipelines[blendMode * 2 + (cullBackFaces ? 1 : 0)].get();
}

void ParticleRendererVulkan::_renderParticle(VkCommandBuffer cmd, uint32_t currentFrame)
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

    vkCmdBeginRendering(cmd, &renderingInfo);

    renderDeviceVulkan->setViewport(width, height);
    renderDeviceVulkan->setScissor(width, height);

    VulkanPipeline* boundPipeline = nullptr;
    for(const ActiveEmitter& active : activeEmitters) {
        _drawEmitter(cmd, active, descriptorSets[currentFrame], boundPipeline);
    }

    vkCmdEndRendering(cmd);
}

void ParticleRendererVulkan::_drawEmitter(VkCommandBuffer cmd, const ActiveEmitter& active, VkDescriptorSet cameraSet, VulkanPipeline*& boundPipeline)
{
    VulkanPipeline* pipeline = _getPipeline(active.blendMode, active.cullBackFaces);
    if(pipeline != boundPipeline) {
        auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, &cameraSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 1, 1, &bindlessSet, 0, nullptr);
        boundPipeline = pipeline;
    }

    pushConstant.containerIdx = active.containerIdx;
    pushConstant.particleCount = active.capacity;
    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ParticlePushConstant), &pushConstant);

    // instance count is however many particles survived the simulate pass
    VkBuffer drawBuffer = static_cast<VkBuffer>(*bufferManagerVulkan->getBuffer(drawCommandsID));
    VkDeviceSize offset = active.containerIdx * sizeof(VkDrawIndirectCommand);
    vkCmdDrawIndirect(cmd, drawBuffer, offset, 1, sizeof(VkDrawIndirectCommand));
}

void ParticleRendererVulkan::_uploadPreviewCamera(uint32_t frame)
{
    // same conventions RendererManagerVulkan uses for the main camera
    Common::UniformBufferObject ubo{};
    ubo.invNormal = glm::mat4(1.0f);
    ubo.view = preview.view;
    ubo.proj = preview.projection;
    ubo.proj[1][1] *= -1.0f;
    ubo.prevViewProj = ubo.proj * ubo.view;
    ubo.cameraPos = glm::vec4(preview.cameraPosition, 1.0f);
    ubo.invView = glm::inverse(preview.view);
    ubo.invProj = glm::inverse(ubo.proj);
    ubo.width = static_cast<float>(preview.width);
    ubo.height = static_cast<float>(preview.height);
    previewUBOs[frame]->update(&ubo, sizeof(ubo));
}

void ParticleRendererVulkan::_renderPreview(VkCommandBuffer cmd, uint32_t currentFrame)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = previewColor->textureImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { { preview.clearColor.r, preview.clearColor.g, preview.clearColor.b, preview.clearColor.a } };

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = previewDepth->textureImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {preview.width, preview.height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    // always clears, the emitter may have nothing to draw this frame
    vkCmdBeginRendering(cmd, &renderingInfo);
    renderDeviceVulkan->setViewport(preview.width, preview.height);
    renderDeviceVulkan->setScissor(preview.width, preview.height);

    VulkanPipeline* boundPipeline = nullptr;
    for(const ActiveEmitter& active : activeEmitters) {
        if(active.containerIdx == preview.containerID) {
            _drawEmitter(cmd, active, previewDescriptorSets[currentFrame], boundPipeline);
        }
    }

    vkCmdEndRendering(cmd);
}


void ParticleRendererVulkan::_uploadContainerRefs()
{
    size_t count = std::min<size_t>(containerRefs.size(), MAX_CONTAINERS);
    bufferManagerVulkan->updateBufferDeviceAddress(containersRefID, containerRefs.data(), count * sizeof(ContainerRefs));
}

void ParticleRendererVulkan::_uploadEmitters(uint32_t frame)
{
    size_t count = std::min<size_t>(std::max<size_t>(containerRefs.size(), 1), MAX_CONTAINERS);
    bufferManagerVulkan->updateBufferDeviceAddress(emittersRefIDs[frame], emittersGPU.data(), count * sizeof(EmitterGPU));
}

bool ParticleRendererVulkan::_isValidContainer(uint32_t containerID) const
{
    return containerID != 0 && containerID < containerRefs.size() && containerRefs[containerID].particlesRef != 0;
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
        Camera* camera = SceneManager::cameraController;

        if(!camera) {
            return;
        }

        render(*camera);
    };

    graph.addPass(node);

    _addPreviewToGraph(graph);
}

void ParticleRendererVulkan::_addPreviewToGraph(RenderGraphVulkan& graph)
{
    // the editor asks again every frame it is visible, which textures the pass uses is decided here before importing
    preview = particleManager->consumePreview();
    previewActive = preview.enabled && preview.width > 0 && preview.height > 0 && _isValidContainer(preview.containerID);
    if(!previewActive) {
        particleManager->setPreviewTextureID(0);
        return;
    }

    auto createTarget = [&](const char* name, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlagBits aspect) {
        RGTextureDesc desc{};
        desc.config.width = preview.width;     // fixed size, the graph recreates it when the editor resizes
        desc.config.height = preview.height;
        desc.config.format = format;
        desc.config.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.config.aspectBits = aspect;
        desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        return graph.createRenderTexture(name, desc);
    };

    RGHandle color = createTarget("ParticlePreview.Color", VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    RGHandle depth = createTarget("ParticlePreview.Depth", depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    previewColor = graph.getTexture(color);
    previewDepth = graph.getTexture(depth);
    particleManager->setPreviewTextureID(previewColor->id());

    // runs after the main particle pass, which already simulated this frame
    RenderPassNode node{};
    node.name = "ParticlePreview";
    node.writes = {
        { color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
        { depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
    };
    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        _renderPreview(cmd, currentFrame);
    };

    graph.addPass(node);
}
