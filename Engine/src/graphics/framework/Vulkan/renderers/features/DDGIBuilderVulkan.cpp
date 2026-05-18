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

void DDGIBuilderVulkan::writeBlendProbe(VkCommandBuffer cmd, uint32_t currentFrame)
{
    pushConstantBlend.probesPerDimension = 8;
    pushConstantBlend.probesResolution = PROBE_RES;
    pushConstantBlend.numRaysPerProbe = raysPerProbe;

    atlasTexture->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    currentVisibilityAtlas->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    
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
    
    atlasTexture->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    currentVisibilityAtlas->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    prevAtlasTexture->copyFrom(cmd, atlasTexture);
    lastFrameVisibilityAtlas->copyFrom(cmd, currentVisibilityAtlas);
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
        pushConstant.probRef = buffer->getAddress();
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
    createTexture(atlasTexture, atlasW, atlasH, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    createTexture(prevAtlasTexture, atlasW, atlasH, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    createTexture(currentVisibilityAtlas, atlasW, atlasH, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    createTexture(lastFrameVisibilityAtlas, atlasW, atlasH, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
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
    auto tlas = raytracer->m_rtBuilder.getTlas();

    auto probTraceDescriptorSets = descriptorManagerVulkan->getDescriptorSet(probTracePipelineSetID);
	DescriptorWriter writer {{}, probTraceDescriptorSets[index] };
	descriptorManagerVulkan->writeUniform2(writer, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeAccelStruct2(writer, postBindings, tlas->getDescAccelStructInfo());
	descriptorManagerVulkan->writeStorageImage2(writer, rayColorBuffer->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeStorageImage2(writer, rayDistanceBuffer->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeImage2(writer, prevAtlasTexture->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeImage2(writer, lastFrameVisibilityAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
	
    auto blendDescriptorSets = descriptorManagerVulkan->getDescriptorSet(blendPipelineSetID);
	DescriptorWriter writerBlend {{}, blendDescriptorSets[index] };
	descriptorManagerVulkan->writeUniform2(writerBlend, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeStorageImage2(writerBlend, atlasTexture->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeImage2(writerBlend, prevAtlasTexture->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeImage2(writerBlend, rayColorBuffer->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeImage2(writerBlend, rayDistanceBuffer->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeStorageImage2(writerBlend, currentVisibilityAtlas->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeImage2(writerBlend, lastFrameVisibilityAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->updateDescriptorSets(&writerBlend.writes);
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
