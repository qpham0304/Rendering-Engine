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
#include <graphics/framework/Vulkan/resources/buffers/AccelStructureBufferVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <window/AppWindow.h>
#include <imgui/imgui.h>

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
	ubo.width  = (float)rayBufferW;
    ubo.height = (float)rayBufferH;
    
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
    auto& transform = lightProbeEntity.getComponent<TransformComponent>();
    
	size_t buffersize = MAX_INSTANCES * sizeof(ObjectDesc);
	bufferManagerVulkan->updateBufferDeviceAddress(raytracer->objDeviceAddressBufferID, raytracer->objects.data(), buffersize);


	raytracer->updateTlas();

	lastViewProj = ubo.proj * ubo.view;

    renderDeviceVulkan->beginLabel(cmd, "DDGI probe render", {1.0, 1.0, 0.0, 1.0});
    writeTrace(cmd, currentFrame);
    writeUpdateVisibility(cmd, currentFrame);
    writeUpdateIrradiance(cmd, currentFrame);
    renderDeviceVulkan->endLabel(cmd);
}

void DDGIBuilderVulkan::writeTrace(VkCommandBuffer cmd, uint32_t currentFrame)
{
    rayColorBuffer->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    rayDistanceBuffer->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

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

    rayColorBuffer->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    rayDistanceBuffer->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


void DDGIBuilderVulkan::writeUpdateIrradiance(VkCommandBuffer cmd, uint32_t currentFrame)
{
    rayColorBuffer->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    currentIrradianceAtlas->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    
    updateIrradiancePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(updateIrradianceSetID)[currentFrame];
	auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
	std::vector<VkDescriptorSet> sets = { descriptorSet, bindlessSet };


	vkCmdPushConstants(cmd, updateIrradiancePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &pushConstant);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        updateIrradiancePipeline->pipelineLayout, 0, 
        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr
    );

    vkCmdDispatch(cmd, totalProbes, 1, 1);

    rayColorBuffer->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    currentIrradianceAtlas->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    lastframeIrradianceAtlas->copyFrom(cmd, currentIrradianceAtlas);
}

void DDGIBuilderVulkan::writeUpdateVisibility(VkCommandBuffer cmd, uint32_t currentFrame)
{
    rayDistanceBuffer->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    currentVisibilityAtlas->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    
    updateVisibilityPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(updateVisibilitySetID)[currentFrame];
	auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
	std::vector<VkDescriptorSet> sets = { descriptorSet, bindlessSet };

	vkCmdPushConstants(cmd, updateVisibilityPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &pushConstant);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        updateVisibilityPipeline->pipelineLayout, 0, 
        static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr
    );

    // float groupX = (atlasW + 7) / 8;
    // float groupY = (atlasH + 7) / 8;
    // vkCmdDispatch(cmd, groupX, groupY, 1);
    vkCmdDispatch(cmd, totalProbes, 1, 1);
    
    rayDistanceBuffer->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    currentVisibilityAtlas->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    lastFrameVisibilityAtlas->copyFrom(cmd, currentVisibilityAtlas);
}

void DDGIBuilderVulkan::renderGui()
{
    
}

TextureVulkan *DDGIBuilderVulkan::getAtlasImage()
{
    return lastframeIrradianceAtlas;
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

        size_t probesPerDimension = lightProbeComponent.probesPerDimension;
        pushConstant.objectsRef = raytracer->objDeviceAddress; 
        pushConstant.probRef = buffer->getAddress();
        pushConstant.probesPerDimension = probesPerDimension;
        
        // glm::vec3 jitter = glm::vec3(pc.gridSpacing * 0.5f);
        // pc.gridOrigin = vec4(objectCenter - (halfGridExtent * 2.0f) + jitter, 1.0f);
        pushConstant.gridOrigin = lightProbeComponent.gridOrigin;
        pushConstant.probesPerDimension = lightProbeComponent.probesPerDimension;
        pushConstant.probesResolution = PROBE_RES;
        pushConstant.gridSpacing = lightProbeComponent.spacing;
        pushConstant.padding = 0;

        atlasW = probesPerDimension * PROBE_RES;
        atlasH = probesPerDimension * probesPerDimension * PROBE_RES;

        totalProbes = probesPerDimension * probesPerDimension * probesPerDimension;
        rayBufferW = raysPerProbe;
        rayBufferH = totalProbes;
        
        uint32_t irradianceStride = 8 + 2;
        uint32_t visibilityStride = 16 + 2;
        visAtlasW = probesPerDimension * visibilityStride;
        visAtlasH = probesPerDimension * probesPerDimension * visibilityStride;

    }
    
    auto createTexture = [&] (TextureVulkan*& texture, uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags extraUsage = 0) {
        TextureSamplerConfig samplerConfig = { VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR };
        TextureConfig imageConfig { .width = w, .height = h, .format = format};
		imageConfig.usage |= extraUsage;

        uint32_t id = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
        texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
    };
    
    // Width = Rays, Height = Total Probes
    createTexture(rayColorBuffer, rayBufferW, rayBufferH, VK_FORMAT_R16G16B16A16_SFLOAT);
    createTexture(rayDistanceBuffer, rayBufferW, rayBufferH, VK_FORMAT_R16G16_SFLOAT);
    createTexture(currentIrradianceAtlas, atlasW, atlasH, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    createTexture(lastframeIrradianceAtlas, atlasW, atlasH, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    createTexture(currentVisibilityAtlas, visAtlasW, visAtlasH, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    createTexture(lastFrameVisibilityAtlas, visAtlasW, visAtlasH, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    
    
    // createTexture(rayColorBuffer, rayBufferW, rayBufferH, VK_FORMAT_R16G16B16A16_SFLOAT);
    // createTexture(rayDistanceBuffer, rayBufferW, rayBufferH, VK_FORMAT_R16G16_SFLOAT);

    // // Use irrAtlas sizing here
    // createTexture(currentIrradianceAtlas, irrAtlasW, irrAtlasH, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    // createTexture(lastframeIrradianceAtlas, irrAtlasW, irrAtlasH, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    // // Use visAtlas sizing here
    // createTexture(currentVisibilityAtlas, visAtlasW, visAtlasH, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    // createTexture(lastFrameVisibilityAtlas, visAtlasW, visAtlasH, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
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
		"assets/shaders/spv/ddgiProbeTrace.comp.spv", 
		{ probTracePipelineLayout, bindlessLayout, materialLayout }, 
		sizeof(PushConstant)
	);

    VkDescriptorSetLayout updateIrradianceLayout = descriptorManagerVulkan->getDescriptorLayout(updateIrradianceLayoutID);
	updateIrradiancePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	updateIrradiancePipeline->createComputePipeline(
		"assets/shaders/spv/ddgiUpdateIrradiance.comp.spv", 
		{ updateIrradianceLayout, bindlessLayout, materialLayout }, 
		sizeof(PushConstant)
	);

    VkDescriptorSetLayout texelCopyLayout = descriptorManagerVulkan->getDescriptorLayout(updateVisibilityLayoutID);
	updateVisibilityPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	updateVisibilityPipeline->createComputePipeline(
		"assets/shaders/spv/ddgiUpdateVisibility.comp.spv", 
		{ texelCopyLayout, bindlessLayout, materialLayout }, 
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


    std::vector<VkDescriptorSetLayoutBinding> updateIrradianceBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> updateIrradiancePoolSize {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 2},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 1},
	};
    updateIrradianceLayoutID = descriptorManagerVulkan->createLayout(updateIrradianceBindings);
    updateIrradiancePoolID = descriptorManagerVulkan->createPool(updateIrradiancePoolSize, frameCount);
	updateIrradianceSetID = descriptorManagerVulkan->createSets(updateIrradianceLayoutID, updateIrradiancePoolID, frameCount);


    std::vector<VkDescriptorSetLayoutBinding> updateVisibilityBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> updateVisibilityPoolSize {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 2},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 1},
	};
    updateVisibilityLayoutID = descriptorManagerVulkan->createLayout(updateVisibilityBindings);
    updateVisibilityPoolID = descriptorManagerVulkan->createPool(updateVisibilityPoolSize, frameCount);
	updateVisibilitySetID = descriptorManagerVulkan->createSets(updateVisibilityLayoutID, updateVisibilityPoolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void DDGIBuilderVulkan::_updateDescriptor(uint32_t index)
{
    auto tlas = raytracer->m_rtBuilder.getTlas();

    auto probTraceDescriptorSets = descriptorManagerVulkan->getDescriptorSet(probTracePipelineSetID);
	DescriptorWriter writer {{}, probTraceDescriptorSets[index] };
	descriptorManagerVulkan->writeUniform2(writer, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeAccelStruct2(writer, postBindings, tlas->getDescAccelStructInfo());
	descriptorManagerVulkan->writeStorageImage2(writer, rayColorBuffer->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeStorageImage2(writer, rayDistanceBuffer->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeImage2(writer, lastframeIrradianceAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeImage2(writer, lastFrameVisibilityAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
	
    auto updateIrradianceDescriptorSets = descriptorManagerVulkan->getDescriptorSet(updateIrradianceSetID);
	DescriptorWriter writerUpdateIrradiance {{}, updateIrradianceDescriptorSets[index] };
	descriptorManagerVulkan->writeUniform2(writerUpdateIrradiance, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeStorageImage2(writerUpdateIrradiance, rayColorBuffer->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeImage2(writerUpdateIrradiance, lastframeIrradianceAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeStorageImage2(writerUpdateIrradiance, currentIrradianceAtlas->getDescImageInfoGeneral());
	descriptorManagerVulkan->updateDescriptorSets(&writerUpdateIrradiance.writes);
	
    auto updateVisibilityDescriptorSets = descriptorManagerVulkan->getDescriptorSet(updateVisibilitySetID);
	DescriptorWriter writerUpdateVisibility {{}, updateVisibilityDescriptorSets[index] };
	descriptorManagerVulkan->writeUniform2(writerUpdateVisibility, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeStorageImage2(writerUpdateVisibility, rayDistanceBuffer->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeImage2(writerUpdateVisibility, lastFrameVisibilityAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeStorageImage2(writerUpdateVisibility, currentVisibilityAtlas->getDescImageInfoGeneral());
	descriptorManagerVulkan->updateDescriptorSets(&writerUpdateVisibility.writes);
}

void DDGIBuilderVulkan::_recreateResources()
{
    m_logger->warn("recource recreation unimlemented");
}

void DDGIBuilderVulkan::_cleanupResources()
{
	m_logger->warn("recource cleanup unimlemented");
}

bool DDGIBuilderVulkan::onClose()
{
    probTracePipeline->destroy();
    updateIrradiancePipeline->destroy();
    updateVisibilityPipeline->destroy();
    
    return true;
}
