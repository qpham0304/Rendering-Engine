#include "DDGIBuilderVulkan.h"

#include "core/features/Timer.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ApplicationRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/ForwardRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/BloomPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/TemporalPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ShadowMapPassVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/features/ImageBasedVulkan.h"
#include "graphics/framework/vulkan/renderers/features/DDGIBuilderVulkan.h"
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include <graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <window/AppWindow.h>

DDGIBuilderVulkan::DDGIBuilderVulkan()
{
}

DDGIBuilderVulkan::~DDGIBuilderVulkan()
{
}

bool DDGIBuilderVulkan::init(WindowConfig config)
{
    RendererVulkan::init(config);

    
    
    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("RayTraceRendererVulkan");
	raytracer = dynamic_cast<RayTraceRendererVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ShadowMapPassVulkan");
	shadowMapPass = dynamic_cast<ShadowMapPassVulkan*>(renderer);
    assert(raytracer && "failed to retrieve raytracing resource");


    _createResources();
    _createDescriptor();
    _createPipeline();

    return true;
}

void DDGIBuilderVulkan::onUpdate()
{
}

void DDGIBuilderVulkan::render(Camera &camera)
{
	RendererVulkan::render(camera);
	
	if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

	// instanceData.clear(); 
	// objects.clear();
    // lights.clear();

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}

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
    // ubo.color = glm::vec4(1.0, 1.0, 1.0, 1.0);
	ubo.width  = rayBufferW;
    ubo.height = rayBufferH;
    
    bool shouldClear = false;//camera.isMoving();
    ubo.frameSeed = !shouldClear ? rand() % 32768 : ubo.frameSeed;
    ubo.frameCount += 1;
    ubo.clear = shouldClear ? 1 : 0;
	
    pushConstant.lightDir = glm::vec4(shadowMapPass->lightDir, 1.0);
	
	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

    auto entities = scene->getEntitiesWith<LightProbeComponent>();
    Entity lightProbeEntity = entities[0];
    auto& lightProbeComponent = lightProbeEntity.getComponent<LightProbeComponent>();

    
	size_t buffersize = MAX_INSTANCES * sizeof(ObjectDesc);
	bufferManagerVulkan->updateBufferDeviceAddress(raytracer->objDeviceAddressBufferID, raytracer->objects.data(), buffersize);


	raytracer->updateTlas();

	lastViewProj = ubo.proj * ubo.view;

    renderDeviceVulkan->beginLabel(cmd, "DDGI probe render", {1.0, 1.0, 0.0, 1.0});
    writeTraceProbe(cmd, currentFrame);
    writeBlendProbe(cmd, currentFrame);
    renderDeviceVulkan->endLabel(cmd);

	rendererManagerVulkan->setDisplayImage(atlasTexture);
}

void DDGIBuilderVulkan::writeTraceProbe(VkCommandBuffer cmd, uint32_t currentFrame)
{
	TextureManagerVulkan::transitionImageLayout(
		cmd, rayColorBuffer->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan
	);
    TextureManagerVulkan::transitionImageLayout(
		cmd, rayDistanceBuffer->textureImage, VK_FORMAT_R16G16_SFLOAT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan
	);

	probTracePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
	auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(probTracePipelineSetID)[currentFrame];
	auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
	std::vector<VkDescriptorSet> sets = { descriptorSet, bindlessSet };

	vkCmdPushConstants(cmd, probTracePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &pushConstant);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        probTracePipeline->pipelineLayout, 0, 
        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr
    );

    vkCmdDispatch(cmd, totalProbes, 1, 1);

	TextureManagerVulkan::transitionImageLayout(
		cmd, rayColorBuffer->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
	);
    TextureManagerVulkan::transitionImageLayout(
		cmd, rayDistanceBuffer->textureImage, VK_FORMAT_R16G16_SFLOAT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
	);
}

void DDGIBuilderVulkan::writeBlendProbe(VkCommandBuffer cmd, uint32_t currentFrame)
{
    pushConstantBlend.probesPerDimension = 8;
    pushConstantBlend.probesResolution = PROBE_RES;
    pushConstantBlend.numRaysPerProbe = raysPerProbe;

    TextureManagerVulkan::transitionImageLayout(
		cmd, atlasTexture->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan
	);

    TextureManagerVulkan::transitionImageLayout(
		cmd, currentVisibilityAtlas->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 1, 1, renderDeviceVulkan
	);
    
    blendPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(blendPipelineSetID)[currentFrame];
	auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
	std::vector<VkDescriptorSet> sets = { descriptorSet, bindlessSet };

	vkCmdPushConstants(cmd, blendPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantBlend), &pushConstantBlend);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        blendPipeline->pipelineLayout, 0, 
        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr
    );

    float groupX = (atlasW + 7) / 8;
    float groupY = (atlasH + 7) / 8;
    vkCmdDispatch(cmd, groupX, groupY, 1);
    
    TextureManagerVulkan::transitionImageLayout(
		cmd, atlasTexture->textureImage, VK_FORMAT_R16G16_SFLOAT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
	);

    TextureManagerVulkan::transitionImageLayout(
		cmd, currentVisibilityAtlas->textureImage, VK_FORMAT_R16G16_SFLOAT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan
	);

    TextureManagerVulkan::copyImage(
        cmd, atlasTexture, prevAtlasTexture, 
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
        atlasW, atlasH, renderDeviceVulkan
    );

    TextureManagerVulkan::copyImage(
        cmd, currentVisibilityAtlas, lastFrameVisibilityAtlas, 
        VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
        atlasW, atlasH, renderDeviceVulkan
    );
}

TextureVulkan *DDGIBuilderVulkan::getAtlasImage()
{
    // return atlasTexture;
    return prevAtlasTexture;
}

TextureVulkan *DDGIBuilderVulkan::getVisibilityAtlasImage()
{
    return lastFrameVisibilityAtlas;
}

void DDGIBuilderVulkan::_createResources()
{
	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}

    //TODO: only assume one probe grid for the entire scene at the moment
    auto entities = scene->getEntitiesWith<LightProbeComponent>();

    if(entities.size() == 0) {
		m_logger->error("No entity with light probe component");
    } else {
        Entity lightProbeEntity = entities[0];  //NOTE: only assume there's only one prob grid
        auto& lightProbeComponent = lightProbeEntity.getComponent<LightProbeComponent>();

        
        bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));
            
        uint32_t bufferID = bufferManagerVulkan->createBufferDeviceAddress(lightProbeComponent.bufferSize);
        auto buffer = dynamic_cast<DeviceAddressBufferVulkan*>(bufferManagerVulkan->getBuffer(bufferID));
        assert(buffer && "failed to retrieve vulkan buffer");
        assert(lightProbeComponent.probeGrid.size() > 0 && lightProbeComponent.bufferSize > 0 && "probe component has no size");
        
        buffer->update(lightProbeComponent.probeGrid.data(), lightProbeComponent.bufferSize);

        pushConstant.objectsRef = raytracer->objDeviceAddress; 
        pushConstant.probRef = buffer->getReference();
        pushConstant.probesPerDimension = lightProbeComponent.probesPerDimension;
        pushConstant.probesResolution = PROBE_RES;
        pushConstant.gridOrigin = lightProbeComponent.gridOrigin;
        pushConstant.gridSpacing = lightProbeComponent.spacing;

        size_t probesPerDimension = lightProbeComponent.probesPerDimension;
        atlasW = probesPerDimension * PROBE_RES;
        atlasH = probesPerDimension * probesPerDimension * PROBE_RES;

        totalProbes = probesPerDimension * probesPerDimension * probesPerDimension;
        rayBufferW = raysPerProbe;
        rayBufferH = totalProbes;
    }
    
    auto createTexture = [&] (uint32_t& id, TextureVulkan*& texture, uint32_t w, uint32_t h, VkFormat format) {
        id = textureManagerVulkan->createTexture();
        texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
        
        assert(texture && "failed to cast texture into vulkan texture");
        
        TextureManagerVulkan::createImage(
            w, h, format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture->textureImage,
            texture->textureImageMemory,
            1,
            renderDeviceVulkan->device
        );

        TextureManagerVulkan::createImageView(
            texture->textureImage, texture->textureImageView,
            format, VK_IMAGE_ASPECT_COLOR_BIT, 1, renderDeviceVulkan->device
        );

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        TextureManagerVulkan::createTextureSampler(
            texture->textureSampler, 
            renderDeviceVulkan->device,
            samplerInfo
        );

        
    auto cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
        TextureManagerVulkan::transitionImageLayout(
            cmd, rayColorBuffer->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    };
    
    // Width = Rays, Height = Total Probes
    createTexture(rayColorBufferID, rayColorBuffer, rayBufferW, rayBufferH, VK_FORMAT_R16G16B16A16_SFLOAT);
    createTexture(rayDistanceBufferID, rayDistanceBuffer, rayBufferW, rayBufferH, VK_FORMAT_R16G16_SFLOAT);
    createTexture(atlasID, atlasTexture, atlasW, atlasH, VK_FORMAT_R16G16B16A16_SFLOAT);
    createTexture(prevAtlasID, prevAtlasTexture, atlasW, atlasH, VK_FORMAT_R16G16B16A16_SFLOAT);
    createTexture(currentVisibilityAtlasID, currentVisibilityAtlas, atlasW, atlasH, VK_FORMAT_R16G16_SFLOAT);
    createTexture(lastFrameVisibilityAtlasID, lastFrameVisibilityAtlas, atlasW, atlasH, VK_FORMAT_R16G16_SFLOAT);

    auto cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();

    TextureManagerVulkan::transitionImageLayout(
        cmd, rayColorBuffer->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(
        cmd, rayDistanceBuffer->textureImage, VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(
        cmd, atlasTexture->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    TextureManagerVulkan::transitionImageLayout(
        cmd, prevAtlasTexture->textureImage, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);
    
    TextureManagerVulkan::transitionImageLayout(
        cmd, currentVisibilityAtlas->textureImage, VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);
    
    TextureManagerVulkan::transitionImageLayout(
        cmd, lastFrameVisibilityAtlas->textureImage, VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, renderDeviceVulkan);

    renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

}

void DDGIBuilderVulkan::_createPipeline()
{
	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

	void* handle = materialManager->getMaterialLayout();
	auto materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

    VkDescriptorSetLayout probTracePipelineLayout = descriptorManagerVulkan->getDescriptorLayout(probTracePipelineLayoutID);
	probTracePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	probTracePipeline->createComputePipeline(
		"assets/shaders/spv/ddgiTrace.comp.spv", 
		{ probTracePipelineLayout, bindlessLayout, materialLayout }, 
		sizeof(PushConstant)
	);

    VkDescriptorSetLayout blendPipelineLayout = descriptorManagerVulkan->getDescriptorLayout(blendPipelineLayoutID);
	blendPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	blendPipeline->createComputePipeline(
		"assets/shaders/spv/ddgiBlend.comp.spv", 
		{ blendPipelineLayout, bindlessLayout, materialLayout }, 
		sizeof(PushConstant)
	);
}

void DDGIBuilderVulkan::_createDescriptor()
{
	uint32_t frameCount = VulkanUtils::numFrames();
    postBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> postPoolSizes {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frameCount * 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 2},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 2},
	};
    probTracePipelineLayoutID = descriptorManagerVulkan->createLayout(postBindings);
    probTracePipelinePoolID = descriptorManagerVulkan->createPool(postPoolSizes, frameCount);
	probTracePipelineSetID = descriptorManagerVulkan->createSets(probTracePipelineLayoutID, probTracePipelinePoolID, frameCount);

    std::vector<VkDescriptorSetLayoutBinding> blendBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> blendPoolSize {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 2},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 4},
	};
    blendPipelineLayoutID = descriptorManagerVulkan->createLayout(blendBindings);
    blendPipelinePoolID = descriptorManagerVulkan->createPool(blendPoolSize, frameCount);
	blendPipelineSetID = descriptorManagerVulkan->createSets(blendPipelineLayoutID, blendPipelinePoolID, frameCount);


    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void DDGIBuilderVulkan::_updateDescriptor(uint32_t index)
{
    auto tlas = raytracer->m_rtBuilder.getAccelerationStructure();

	VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[index]);
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSetAccelerationStructureKHR descASInfo{};
	descASInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descASInfo.accelerationStructureCount = 1;
    descASInfo.pAccelerationStructures    = &tlas;

	VkDescriptorImageInfo rayColorBufferInfo{};
	rayColorBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	rayColorBufferInfo.imageView = rayColorBuffer->textureImageView;
	rayColorBufferInfo.sampler = rayColorBuffer->textureSampler;

	VkDescriptorImageInfo rayDistanceBufferInfo{};
	rayDistanceBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	rayDistanceBufferInfo.imageView = rayDistanceBuffer->textureImageView;
	rayDistanceBufferInfo.sampler = rayDistanceBuffer->textureSampler;

	VkDescriptorImageInfo prevAtlasInfo{};
	prevAtlasInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	prevAtlasInfo.imageView = prevAtlasTexture->textureImageView;
	prevAtlasInfo.sampler = prevAtlasTexture->textureSampler;
    
	VkDescriptorImageInfo visibilityAtlasInfo{};
	visibilityAtlasInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	visibilityAtlasInfo.imageView = lastFrameVisibilityAtlas->textureImageView;
	visibilityAtlasInfo.sampler = lastFrameVisibilityAtlas->textureSampler;

    auto probTraceDescriptorSets = descriptorManagerVulkan->getDescriptorSet(probTracePipelineSetID);
	std::vector<VkWriteDescriptorSet> writePostProcess;
	descriptorManagerVulkan->writeUniform(&writePostProcess, probTraceDescriptorSets[index], 0, bufferInfo);
	descriptorManagerVulkan->writeAccelStruct(&writePostProcess, probTraceDescriptorSets[index], 1, postBindings, descASInfo);
	descriptorManagerVulkan->writeStorageImage(&writePostProcess, probTraceDescriptorSets[index], 2, rayColorBufferInfo);
	descriptorManagerVulkan->writeStorageImage(&writePostProcess, probTraceDescriptorSets[index], 3, rayDistanceBufferInfo);
	descriptorManagerVulkan->writeImage(&writePostProcess, probTraceDescriptorSets[index], 4, prevAtlasInfo);
	descriptorManagerVulkan->writeImage(&writePostProcess, probTraceDescriptorSets[index], 5, visibilityAtlasInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writePostProcess);

    //blend pipeline
	VkDescriptorImageInfo atlasBufferInfo{};
	atlasBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	atlasBufferInfo.imageView = atlasTexture->textureImageView;
	atlasBufferInfo.sampler = atlasTexture->textureSampler;

	VkDescriptorImageInfo prevAtlasBufferInfo{};
	prevAtlasBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	prevAtlasBufferInfo.imageView = prevAtlasTexture->textureImageView;
	prevAtlasBufferInfo.sampler = prevAtlasTexture->textureSampler;

    VkDescriptorImageInfo rayColorBufferSamplerInfo{};
	rayColorBufferSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	rayColorBufferSamplerInfo.imageView = rayColorBuffer->textureImageView;
	rayColorBufferSamplerInfo.sampler = rayColorBuffer->textureSampler;
    
	VkDescriptorImageInfo rayDistanceBufferSamplerInfo{};
	rayDistanceBufferSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	rayDistanceBufferSamplerInfo.imageView = rayDistanceBuffer->textureImageView;
	rayDistanceBufferSamplerInfo.sampler = rayDistanceBuffer->textureSampler;
    
	VkDescriptorImageInfo currentVisibilityAtlasInfo{};
	currentVisibilityAtlasInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	currentVisibilityAtlasInfo.imageView = currentVisibilityAtlas->textureImageView;
	currentVisibilityAtlasInfo.sampler = currentVisibilityAtlas->textureSampler;

	VkDescriptorImageInfo lastFrameVisibilityAtlasInfo{};
	lastFrameVisibilityAtlasInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	lastFrameVisibilityAtlasInfo.imageView = lastFrameVisibilityAtlas->textureImageView;
	lastFrameVisibilityAtlasInfo.sampler = lastFrameVisibilityAtlas->textureSampler;
    
    auto blendDescriptorSets = descriptorManagerVulkan->getDescriptorSet(blendPipelineSetID);
	std::vector<VkWriteDescriptorSet> writeBlend;
	descriptorManagerVulkan->writeUniform(&writeBlend, blendDescriptorSets[index], 0, bufferInfo);
	descriptorManagerVulkan->writeStorageImage(&writeBlend, blendDescriptorSets[index], 1, atlasBufferInfo);
	descriptorManagerVulkan->writeImage(&writeBlend, blendDescriptorSets[index], 2, prevAtlasBufferInfo);
	descriptorManagerVulkan->writeImage(&writeBlend, blendDescriptorSets[index], 3, rayColorBufferSamplerInfo);
	descriptorManagerVulkan->writeImage(&writeBlend, blendDescriptorSets[index], 4, rayDistanceBufferSamplerInfo);
	descriptorManagerVulkan->writeStorageImage(&writeBlend, blendDescriptorSets[index], 5, currentVisibilityAtlasInfo);
	descriptorManagerVulkan->writeImage(&writeBlend, blendDescriptorSets[index], 6, lastFrameVisibilityAtlasInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writeBlend);
}

void DDGIBuilderVulkan::_recreateResources()
{
}

bool DDGIBuilderVulkan::onClose()
{
    probTracePipeline->destroy();
    blendPipeline->destroy();
    
    return true;
}
