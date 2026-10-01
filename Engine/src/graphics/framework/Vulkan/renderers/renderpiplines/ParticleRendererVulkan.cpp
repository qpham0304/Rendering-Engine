#include "ParticleRendererVulkan.h"

#include <window/AppWindow.h>
#include <particle/ParticleManager.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
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

    particleManager = &ServiceLocator::GetService<ParticleManager>("ParticleManager");
    containersRefID = bufferManagerVulkan->createBufferDeviceAddress(MAX_CONTAINERS * sizeof(ContainerRefs));
    bufferManagerVulkan->updateBufferDeviceAddress(containersRefID, containerRefs.data(), MAX_CONTAINERS * sizeof(ContainerRefs));
    
    containersRef = bufferManagerVulkan->getBuffer(containersRefID)->getAddress();
    pushConstant.containersRef = containersRef;

    bufferManagerVulkan->createUniformBuffers(emitterUniformBuffersList, sizeof(EmitterUBO));

    _createResources();
    _createDescriptor();
    _createPipelines();

    return true;
}

bool ParticleRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();

	_createPipelines();

    return true;
}

void ParticleRendererVulkan::onUpdate()
{
    auto* particleManager = &ServiceLocator::GetService<ParticleManager>("ParticleManager");
    const auto& containers = particleManager->getAllContainerData();

    //recreate the entire bda reference table to match the particleManager's container data
    if(containerRefs.size() != containers.size()) {
        m_logger->info("count repeat check");
        containerRefs.clear();
        containerRefs.push_back({});    // location 1 for default value and debug
        for(int i = 1; i < containers.size(); i++) {
            const auto& container = containers[i];
            ContainerRefs refs{};
            refs.lifetimeBufferRef = bufferManagerVulkan->getBuffer(container.lifetimeBufferID)->getAddress();
            refs.positionsBufferRef = bufferManagerVulkan->getBuffer(container.positionsBufferID)->getAddress();
            refs.scalesBufferRef = bufferManagerVulkan->getBuffer(container.scalesBufferID)->getAddress();
            refs.velocitiesBufferRef = bufferManagerVulkan->getBuffer(container.velocitiesBufferID)->getAddress();
            refs.colorsBufferRef = bufferManagerVulkan->getBuffer(container.colorsBufferID)->getAddress();
            containerRefs.push_back(refs);
        }
        bufferManagerVulkan->updateBufferDeviceAddress(containersRefID, containerRefs.data(), MAX_CONTAINERS * sizeof(ContainerRefs));
    }

}

void ParticleRendererVulkan::render(Camera &camera)
{
    Timer timer(m_name, true);

    RendererVulkan::_resize();
    rendererManagerVulkan->setDisplayImage(outTexture);

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();

    pushConstant.deltaTime = AppWindow::getDeltaTime();

    renderDeviceVulkan->beginLabel(cmd, "Particle Compute Pass", {1.0, 1.0, 0.5, 1.0});
    _computeParticle(cmd, currentFrame, scene);
    renderDeviceVulkan->endLabel(cmd);
    
    renderDeviceVulkan->beginLabel(cmd, "Particle Render Pass", {1.0, 0.0, 1.0, 1.0});
    _renderParticle(cmd, currentFrame, scene);
	renderDeviceVulkan->endLabel(cmd);
    
}

void ParticleRendererVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();

    _createResources();
    _createPipelines();
    _updateDescriptor();
}

void ParticleRendererVulkan::_cleanupResources()
{
    TextureManager& textureManager = ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    textureManager.destroy(outTexture->id());
    pipeline->destroy();
}

void ParticleRendererVulkan::_createResources()
{
    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;

    auto createTexture = [&] (uint32_t& id){
        id = textureManagerVulkan->createTexture();
        TextureVulkan* texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
        
        assert(texture && "failed to cast texture into vulkan texture");
        
        
        TextureManagerVulkan::createImage(
            swapchain.swapChainExtent.width,
            swapchain.swapChainExtent.height,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture->textureImage,
            texture->textureImageMemory,
            1,
            renderDeviceVulkan->device
        );

        //TODO: remove general creation or change this to RGBA16FLOAT
        TextureManagerVulkan::createImageView(
            texture->textureImage,
            texture->textureImageView,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1,
            renderDeviceVulkan->device
        );

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        TextureManagerVulkan::createTextureSampler(
            texture->textureSampler, 
            renderDeviceVulkan->device,
            samplerInfo
        );

        VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
        texture->transitImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

		return texture;
    };
    
	uint32_t textureID;
	outTexture = createTexture(textureID);
	rendererManagerVulkan->addRenderTexture("particleColorOut", textureID);

    // uint32_t depthTextureID = textureManagerVulkan->createTexture();
    // depthTexture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(depthTextureID));
	// VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

	// TextureManagerVulkan::createImage(
	// 	swapchain.swapChainExtent.width,
	// 	swapchain.swapChainExtent.height,
	// 	depthFormat,
	// 	VK_IMAGE_TILING_OPTIMAL,
	// 	VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	// 	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	// 	depthTexture->textureImage,
	// 	depthTexture->textureImageMemory,
	// 	1,
	// 	renderDeviceVulkan->device
	// );
	
	// TextureManagerVulkan::createImageView(depthTexture->textureImage,
    //     depthTexture->textureImageView,
    //     depthFormat,
    //     VK_IMAGE_ASPECT_DEPTH_BIT,
    //     1,
    //     renderDeviceVulkan->device
    // );

	// rendererManagerVulkan->addRenderTexture("particleDepthOut", textureID);

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

	AttachmentsInfo attachmentsInfo{};
    attachmentsInfo.colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    attachmentsInfo.depthAttachmentFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
    attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;


    VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
	VkDescriptorPool descriptorPool = descriptorManagerVulkan->getDescriptorPool(poolID);

	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

	void* handle = materialManager->getMaterialLayout();
	auto materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);


    pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createGraphicsPipelineDynamic(
        "assets/shaders/spv/particle.vert.spv",
        "assets/shaders/spv/particle.frag.spv",
        config,
        attachmentsInfo,
        vertexInputInfo,
        { descriptorSetLayout, bindlessLayout, materialLayout },
        sizeof(pushConstant)
    );

    computePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    computePipeline->createComputePipeline(
        "assets/shaders/spv/particle.comp.spv",
        { descriptorSetLayout, bindlessLayout, materialLayout },
        sizeof(pushConstant)
    );
}

void ParticleRendererVulkan::_createDescriptor()
{
    layoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 2},
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
		descriptorManagerVulkan->writeUniform2(writer, emitterUniformBuffersList[i]->getDescUniformBufferInfo());
		descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
	}
}

void ParticleRendererVulkan::_computeParticle(VkCommandBuffer cmd, uint32_t currentFrame, Scene* scene)
{
    computePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
        ParticleContainer container = particleManager->getContainer(emitter.containerID);
        ParticleManager::ContainerData containerData = particleManager->getContainerData(emitter.containerID);

        emitterUBO.emitMax = emitter.emitMax;
        emitterUBO.emitCount = emitter.emitCount;
        emitterUBO.areRecycled = emitter.areRecycled;
        emitterUBO.emitAccumulator = emitter.emitAccumulator;
        emitterUBO.emitRate = emitter.emitRate;
        emitterUBO.lifetimeMin = emitter.lifetimeMin;
        emitterUBO.lifetimeMax = emitter.lifetimeMax;
        emitterUBO.speedMin = emitter.speedMin;
        emitterUBO.speedMax = emitter.speedMax;
        emitterUBO.spawnPosition = emitter.spawnPosition;
        emitterUBO.force = emitter.force;
        emitterUBO.resetPosition = emitter.resetPosition;

        emitterUniformBuffersList[currentFrame]->update(&emitterUBO, sizeof(emitterUBO));

        pushConstant.containerIdx = emitter.containerID;
        pushConstant.particleCount = container.m_size;
        vkCmdPushConstants(cmd, computePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlePushConstant), &pushConstant);

        _createBarrier(cmd, containerData.positionsBufferID);
        _createBarrier(cmd, containerData.velocitiesBufferID);
        
        uint32_t groupX_size = 256;
        vkCmdDispatch(cmd, (container.m_size + groupX_size - 1) / groupX_size, 1, 1);
    });
    scene->forEnitiesWith<ParticleEmitter>(func);
}

void ParticleRendererVulkan::_renderParticle(VkCommandBuffer cmd, uint32_t currentFrame, Scene* scene)
{
    outTexture->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = outTexture->textureImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = { {0.1f, 0.1f, 0.1f, 1.0f} };

    std::vector<VkRenderingAttachmentInfo> colorAttachments = { colorAttachment };

    // Optional depth attachment if needed for particle
    // VkRenderingAttachmentInfo depthAttachment{};
    // depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    // depthAttachment.imageView = depthTexture->textureImageView;
    // depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // depthAttachment.clearValue.depthStencil = {1.0f, 0};

    auto ubo = rendererManagerVulkan->getUBO();
    uint32_t width = ubo.width;
    uint32_t height = ubo.height;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {width, height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = nullptr;
    // renderingInfo.pDepthAttachment = &depthAttachment;


    vkCmdBeginRendering(cmd, &renderingInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
    
    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
        ParticleContainer container = particleManager->getContainer(emitter.containerID);
        
        emitterUBO.emitMax = emitter.emitMax;
        emitterUBO.emitCount = emitter.emitCount;
        emitterUBO.areRecycled = emitter.areRecycled;
        emitterUBO.emitAccumulator = emitter.emitAccumulator;
        emitterUBO.emitRate = emitter.emitRate;
        emitterUBO.lifetimeMin = emitter.lifetimeMin;
        emitterUBO.lifetimeMax = emitter.lifetimeMax;
        emitterUBO.speedMin = emitter.speedMin;
        emitterUBO.speedMax = emitter.speedMax;
        emitterUBO.spawnPosition = emitter.spawnPosition;
        emitterUBO.force = emitter.force;
        emitterUBO.resetPosition = emitter.resetPosition;

        emitterUniformBuffersList[currentFrame]->update(&emitterUBO, sizeof(emitterUBO));

        pushConstant.containerIdx = emitter.containerID;
        pushConstant.particleCount = container.m_size;
        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ParticlePushConstant), &pushConstant);

        vkCmdDraw(cmd, container.m_size * 6, 1, 0, 0);  //TODO: use drawindirect with indirect buffer for efficiency ignore for now
    });

    scene->forEnitiesWith<ParticleEmitter>(func);
    
    vkCmdEndRendering(cmd);

    outTexture->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ParticleRendererVulkan::_createBarrier(VkCommandBuffer cmd, uint32_t bufferID)
{
    BufferVulkan* buffer = bufferManagerVulkan->getBuffer(bufferID);
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.buffer = static_cast<VkBuffer>(*buffer);
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        0, 0, nullptr, 1, &barrier, 0, nullptr
    );
}
