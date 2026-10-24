#include "DDGIBuilderVulkan.h"

#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ShadowMapPassVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/features/ImageBasedVulkan.h"
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/Vulkan/resources/buffers/AccelStructureBufferVulkan.h"
#include "graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h"
#include "graphics/framework/Vulkan/renderers/RendererManagerVulkan.h"
#include "graphics/framework/vulkan/core/VulkanPipeline.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/features/Camera.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <random>

//NOTE: only assume there's only one prob grid for the entire scene at the moment
static LightProbeComponent* findProbeGrid()
{
	Scene* scene = SceneManager::getInstance().getActiveScene();
	if(!scene) {
		return nullptr;
	}

	auto entities = scene->getEntitiesWith<LightProbeComponent>();
	if(entities.empty()) {
		return nullptr;
	}

	Entity lightProbeEntity = entities[0];
	return &lightProbeEntity.getComponent<LightProbeComponent>();
}

// LightProbeComponent::probeCounts when it is set, otherwise the same count on every axis
static glm::uvec3 gridProbeCounts(const LightProbeComponent& probeGrid, uint32_t maxCount)
{
	glm::uvec3 counts = probeGrid.probeCounts;
	if(counts.x == 0 || counts.y == 0 || counts.z == 0) {
		counts = glm::uvec3(probeGrid.probesPerDimension);
	}
	return glm::clamp(counts, glm::uvec3(1), glm::uvec3(maxCount));
}

// uniformly distributed rotation so the rays cover every direction over time
static glm::mat4 randomRotation()
{
	static std::mt19937 generator { std::random_device{}() };
	std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

	float u1 = distribution(generator);
	float u2 = distribution(generator) * glm::two_pi<float>();
	float u3 = distribution(generator) * glm::two_pi<float>();
	float a = std::sqrt(1.0f - u1);
	float b = std::sqrt(u1);

	return glm::mat4_cast(glm::quat(b * std::cos(u3), a * std::sin(u2), a * std::cos(u2), b * std::sin(u3)));
}

DDGIBuilderVulkan::DDGIBuilderVulkan()
{
	setFootprint("DDGIBuilderVulkan", sizeof(DDGIBuilderVulkan), MemorySystem::Graphics);
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
	renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
    assert(raytracer && imageBasedRenderer && "failed to retrieve renderer");

	settings = &rendererManagerVulkan->getGraphicsSettings().add<DDGISettings>("DDGI");

	// the scene can gain its probe grid later, render() resizes the atlases when it shows up
	LightProbeComponent* probeGrid = findProbeGrid();
	if(probeGrid) {
		probeCounts = gridProbeCounts(*probeGrid, MAX_PROBES_PER_DIMENSION);
	} else {
		m_logger->warn("No entity with light probe component");
	}

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

	needResize = false;	// nothing in this pass needed resize

	LightProbeComponent* probeGrid = findProbeGrid();
	AccelStructureBufferVulkan* tlas = raytracer->m_rtBuilder.getTlas();
	hasProbe = probeGrid && probeGrid->spacing > 0.0f && tlas;

	if(settings->resetProbes) {
		resetHistory = true;
		settings->resetProbes = false;
	}

	settings->hasProbeGrid = hasProbe;
	settings->probeCounts = hasProbe ? glm::ivec3(probeCounts) : glm::ivec3(0);
	settings->irradianceAtlas = lastframeIrradianceAtlas->id();
	settings->visibilityAtlas = lastFrameVisibilityAtlas->id();

	if(!settings->isReady()) {
		return;
	}

	// probes hold light for the place they were in, a moved grid has to start over
	if(probeGrid->gridOrigin != pushConstant.gridOrigin || probeGrid->spacing != pushConstant.gridSpacing) {
		resetHistory = true;
	}

	pushConstant.objectsRef = rendererManagerVulkan->getObjectsAddress();
	pushConstant.lightsRef = rendererManagerVulkan->getLightsAddress();
	pushConstant.lightCount = rendererManagerVulkan->getLightCount();
	pushConstant.gridOrigin = probeGrid->gridOrigin;
	pushConstant.probeCounts = glm::ivec4(probeCounts, totalProbes);
	pushConstant.probesResolution = IRRADIANCE_RES;
	pushConstant.gridSpacing = probeGrid->spacing;

	// the grid this frame's atlases are built with, the combine pass samples them with it
	settings->gridOrigin = pushConstant.gridOrigin;
	settings->gridSpacing = pushConstant.gridSpacing;
	settings->gridMin = glm::vec3(pushConstant.gridOrigin);
	settings->gridMax = settings->gridMin + glm::vec3(probeCounts - 1u) * pushConstant.gridSpacing;

	// same direction and color the light pass and the shadow map use
	const GraphicsSettingsSerializer& graphicsSettings = rendererManagerVulkan->getGraphicsSettings();
	const ShadowMapSettings* shadowSettings = graphicsSettings.get<ShadowMapSettings>();
	const LightingSettings* lightingSettings = graphicsSettings.get<LightingSettings>();

	glm::vec3 sunDirection = shadowSettings ? shadowSettings->lightDir : glm::vec3(0.0f);
	if(glm::length(sunDirection) < 0.0001f) {
		sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
	}
	ubo.randomRotation = randomRotation();
	ubo.sunDirection = glm::vec4(glm::normalize(sunDirection), 0.0f);
	ubo.sunColor = lightingSettings ? lightingSettings->sunColor * lightingSettings->sunIntensity : glm::vec4(0.0f);
	ubo.maxRayDistance = probeGrid->spacing * 1.5f;	// a bit less than the diagonal of a grid cell
	ubo.clear = resetHistory ? 1 : 0;
	ubo.hysteresis = settings->hysteresis;
	ubo.normalBias = settings->normalBias;
	ubo.skyIntensity = settings->skyIntensity;

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	raytracer->updateTlas();

	// the atlases, the tlas and the environment can all be recreated between frames
	_updateDescriptor(currentFrame);

    renderDeviceVulkan->beginLabel(cmd, "DDGI probe render", {1.0, 1.0, 0.0, 1.0});
    writeTrace(cmd, currentFrame);
    writeUpdateIrradiance(cmd, currentFrame);
    writeUpdateVisibility(cmd, currentFrame);
    renderDeviceVulkan->endLabel(cmd);

	resetHistory = false;
}

void DDGIBuilderVulkan::writeTrace(VkCommandBuffer cmd, uint32_t currentFrame)
{
    _beginWrite(cmd, rayColorBuffer);
    _beginWrite(cmd, rayDistanceBuffer);

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

	// local_size_x of the shader is raysPerProbe, so one group per probe
    vkCmdDispatch(cmd, totalProbes, 1, 1);

    _endWrite(cmd, rayColorBuffer);
    _endWrite(cmd, rayDistanceBuffer);
}

void DDGIBuilderVulkan::writeUpdateIrradiance(VkCommandBuffer cmd, uint32_t currentFrame)
{
    _beginWrite(cmd, currentIrradianceAtlas);

    updateIrradiancePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(updateIrradianceSetID)[currentFrame];

	vkCmdPushConstants(cmd, updateIrradiancePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &pushConstant);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        updateIrradiancePipeline->pipelineLayout, 0,
        1, &descriptorSet, 0, nullptr
    );

    uint32_t groupX = (irrAtlasW + 7) / 8;
    uint32_t groupY = (irrAtlasH + 7) / 8;
    vkCmdDispatch(cmd, groupX, groupY, 1);

    _endWrite(cmd, currentIrradianceAtlas);

    _copyToLastFrame(cmd, currentIrradianceAtlas, lastframeIrradianceAtlas);
}

void DDGIBuilderVulkan::writeUpdateVisibility(VkCommandBuffer cmd, uint32_t currentFrame)
{
    _beginWrite(cmd, currentVisibilityAtlas);

    updateVisibilityPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(updateVisibilitySetID)[currentFrame];

	vkCmdPushConstants(cmd, updateVisibilityPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &pushConstant);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        updateVisibilityPipeline->pipelineLayout, 0,
        1, &descriptorSet, 0, nullptr
    );

    uint32_t groupX = (visAtlasW + 7) / 8;
    uint32_t groupY = (visAtlasH + 7) / 8;
    vkCmdDispatch(cmd, groupX, groupY, 1);

    _endWrite(cmd, currentVisibilityAtlas);

    _copyToLastFrame(cmd, currentVisibilityAtlas, lastFrameVisibilityAtlas);
}

TextureVulkan *DDGIBuilderVulkan::getAtlasImage()
{
    return lastframeIrradianceAtlas;
}

TextureVulkan *DDGIBuilderVulkan::getVisibilityAtlasImage()
{
    return lastFrameVisibilityAtlas;
}

// true when the graph handed back new textures (first call or probe count change)
bool DDGIBuilderVulkan::_createResources()
{
	if(uniformbuffersList.empty()) {
    	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));
	}

	// one tile per probe, +2 for the 1 texel border around every tile
	uint32_t irradianceStride = IRRADIANCE_RES + 2;
	uint32_t visibilityStride = VISIBILITY_RES + 2;
	totalProbes = probeCounts.x * probeCounts.y * probeCounts.z;
	irrAtlasW = probeCounts.x * irradianceStride;
	irrAtlasH = probeCounts.y * probeCounts.z * irradianceStride;
	visAtlasW = probeCounts.x * visibilityStride;
	visAtlasH = probeCounts.y * probeCounts.z * visibilityStride;
	rayBufferW = raysPerProbe;
	rayBufferH = totalProbes;

    // owned by the render graph, sized by the probe count. the graph hands back the same textures every
    // frame and recreates them when the probe count (so the size) changes
    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    uint32_t previousAtlasID = currentIrradianceAtlas ? currentIrradianceAtlas->id() : 0;
    auto createTexture = [&] (const char* name, RGHandle& handle, uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags extraUsage = 0) {
        RGTextureDesc desc{};
        desc.config.width = w;
        desc.config.height = h;
        desc.config.format = format;
        desc.config.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | extraUsage;	// transfer dst for the clear below
        desc.sampler = { VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR };
        handle = graph.createRenderTexture(name, desc);
        return graph.getTexture(handle);
    };

    // Width = Rays, Height = Total Probes
    rayColorBuffer = createTexture("DDGI.RayColor", rayColorHandle, rayBufferW, rayBufferH, VK_FORMAT_R16G16B16A16_SFLOAT);
    rayDistanceBuffer = createTexture("DDGI.RayDistance", rayDistanceHandle, rayBufferW, rayBufferH, VK_FORMAT_R16G16_SFLOAT);
    currentIrradianceAtlas = createTexture("DDGI.Irradiance", currentIrradianceHandle, irrAtlasW, irrAtlasH, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    lastframeIrradianceAtlas = createTexture("DDGI.IrradianceLastFrame", lastIrradianceHandle, irrAtlasW, irrAtlasH, VK_FORMAT_R16G16B16A16_SFLOAT);
    currentVisibilityAtlas = createTexture("DDGI.Visibility", currentVisibilityHandle, visAtlasW, visAtlasH, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    lastFrameVisibilityAtlas = createTexture("DDGI.VisibilityLastFrame", lastVisibilityHandle, visAtlasW, visAtlasH, VK_FORMAT_R16G16_SFLOAT);

    // all six change together, nothing else to do while they are the same textures as last frame
    // (by id, a recreated texture can land at the same address)
    if(currentIrradianceAtlas->id() == previousAtlasID) {
        return false;
    }

	VkClearColorValue clearColor = {{ 0.0f, 0.0f, 0.0f, 0.0f }};
	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
	for(TextureVulkan* texture : {
		rayColorBuffer, rayDistanceBuffer,
		currentIrradianceAtlas, lastframeIrradianceAtlas,
		currentVisibilityAtlas, lastFrameVisibilityAtlas
	}) {
		TextureManagerVulkan::createBarrier(
			cmd, texture->textureImage,
			0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
		);
		vkCmdClearColorImage(cmd, texture->textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
		TextureManagerVulkan::createBarrier(
			cmd, texture->textureImage,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);
	}
	renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

	resetHistory = true;
	return true;
}

void DDGIBuilderVulkan::_createPipeline()
{
	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

    VkDescriptorSetLayout probTracePipelineLayout = descriptorManagerVulkan->getDescriptorLayout(probTracePipelineLayoutID);
	probTracePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	probTracePipeline->createComputePipeline(
		"assets/shaders/spv/ddgiProbeTrace.comp.spv",
		{ probTracePipelineLayout, bindlessLayout },
		sizeof(PushConstant)
	);

    VkDescriptorSetLayout updateIrradianceLayout = descriptorManagerVulkan->getDescriptorLayout(updateIrradianceLayoutID);
	updateIrradiancePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	updateIrradiancePipeline->createComputePipeline(
		"assets/shaders/spv/ddgiUpdateIrradiance.comp.spv",
		{ updateIrradianceLayout },
		sizeof(PushConstant)
	);

    VkDescriptorSetLayout updateVisibilityLayout = descriptorManagerVulkan->getDescriptorLayout(updateVisibilityLayoutID);
	updateVisibilityPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	updateVisibilityPipeline->createComputePipeline(
		"assets/shaders/spv/ddgiUpdateVisibility.comp.spv",
		{ updateVisibilityLayout },
		sizeof(PushConstant)
	);
}

void DDGIBuilderVulkan::_createDescriptor()
{
	uint32_t frameCount = VulkanUtils::numFrames();
    probTraceBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> probTracePoolSizes {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frameCount * 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 2},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 2},
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 1},
	};
    probTracePipelineLayoutID = descriptorManagerVulkan->createLayout(probTraceBindings);
    probTracePipelinePoolID = descriptorManagerVulkan->createPool(probTracePoolSizes, frameCount);
	probTracePipelineSetID = descriptorManagerVulkan->createSets(probTracePipelineLayoutID, probTracePipelinePoolID, frameCount);


    std::vector<VkDescriptorSetLayoutBinding> updateIrradianceBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> updateIrradiancePoolSize {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 3},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
	};
    updateIrradianceLayoutID = descriptorManagerVulkan->createLayout(updateIrradianceBindings);
    updateIrradiancePoolID = descriptorManagerVulkan->createPool(updateIrradiancePoolSize, frameCount);
	updateIrradianceSetID = descriptorManagerVulkan->createSets(updateIrradianceLayoutID, updateIrradiancePoolID, frameCount);


    std::vector<VkDescriptorSetLayoutBinding> updateVisibilityBindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	};
    std::vector<VkDescriptorPoolSize> updateVisibilityPoolSize {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 2},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
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
	descriptorManagerVulkan->writeAccelStruct2(writer, probTraceBindings, tlas->getDescAccelStructInfo());
	descriptorManagerVulkan->writeStorageImage2(writer, rayColorBuffer->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeStorageImage2(writer, rayDistanceBuffer->getDescImageInfoGeneral());
	descriptorManagerVulkan->writeImage2(writer, lastframeIrradianceAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeImage2(writer, lastFrameVisibilityAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeStorage2(writer, imageBasedRenderer->finalSumBuffers[index]->getDescStorageBufferInfo());
	descriptorManagerVulkan->updateDescriptorSets(&writer.writes);

    auto updateIrradianceDescriptorSets = descriptorManagerVulkan->getDescriptorSet(updateIrradianceSetID);
	DescriptorWriter writerUpdateIrradiance {{}, updateIrradianceDescriptorSets[index] };
	descriptorManagerVulkan->writeUniform2(writerUpdateIrradiance, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeImage2(writerUpdateIrradiance, rayColorBuffer->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeImage2(writerUpdateIrradiance, rayDistanceBuffer->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeImage2(writerUpdateIrradiance, lastframeIrradianceAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeStorageImage2(writerUpdateIrradiance, currentIrradianceAtlas->getDescImageInfoGeneral());
	descriptorManagerVulkan->updateDescriptorSets(&writerUpdateIrradiance.writes);

    auto updateVisibilityDescriptorSets = descriptorManagerVulkan->getDescriptorSet(updateVisibilitySetID);
	DescriptorWriter writerUpdateVisibility {{}, updateVisibilityDescriptorSets[index] };
	descriptorManagerVulkan->writeUniform2(writerUpdateVisibility, uniformbuffersList[index]->getDescUniformBufferInfo());
	descriptorManagerVulkan->writeImage2(writerUpdateVisibility, rayDistanceBuffer->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeImage2(writerUpdateVisibility, lastFrameVisibilityAtlas->getDescImageInfoReadOnly());
	descriptorManagerVulkan->writeStorageImage2(writerUpdateVisibility, currentVisibilityAtlas->getDescImageInfoGeneral());
	descriptorManagerVulkan->updateDescriptorSets(&writerUpdateVisibility.writes);
}

// the atlas size follows the probe count, not the window
void DDGIBuilderVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();
	_createResources();

    for(int i = 0; i < VulkanUtils::numFrames(); i++) {
        _updateDescriptor(i);
    }
}

void DDGIBuilderVulkan::_cleanupResources()
{
}

// the textures rest in SHADER_READ_ONLY between passes, these two wrap a compute pass that writes one.
// TextureVulkan::transitImage is not used here because its barriers only cover fragment shader readers
void DDGIBuilderVulkan::_beginWrite(VkCommandBuffer cmd, TextureVulkan* texture)
{
	TextureManagerVulkan::createBarrier(
		cmd, texture->textureImage,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	);
}

void DDGIBuilderVulkan::_endWrite(VkCommandBuffer cmd, TextureVulkan* texture)
{
	TextureManagerVulkan::createBarrier(
		cmd, texture->textureImage,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT
	);
}

void DDGIBuilderVulkan::_copyToLastFrame(VkCommandBuffer cmd, TextureVulkan* current, TextureVulkan* lastFrame)
{
	const VkPipelineStageFlags shaderStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	TextureManagerVulkan::createBarrier(
		cmd, current->textureImage,
		VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT
	);
	// the gui preview samples lastFrame from a fragment shader
	TextureManagerVulkan::createBarrier(
		cmd, lastFrame->textureImage,
		VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		shaderStages, VK_PIPELINE_STAGE_TRANSFER_BIT
	);

	VkImageCopy copyRegion{};
	copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	copyRegion.extent = { current->width(), current->height(), 1 };
	vkCmdCopyImage(
		cmd, current->textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		lastFrame->textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copyRegion
	);

	TextureManagerVulkan::createBarrier(
		cmd, current->textureImage,
		VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
	);
	TextureManagerVulkan::createBarrier(
		cmd, lastFrame->textureImage,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, shaderStages
	);
}

bool DDGIBuilderVulkan::onClose()
{
    rendererManagerVulkan->getGraphicsSettings().remove<DDGISettings>();
    settings = nullptr;

    probTracePipeline->destroy();
    updateIrradiancePipeline->destroy();
    updateVisibilityPipeline->destroy();

    return true;
}

void DDGIBuilderVulkan::addToGraph(RenderGraphVulkan& graph)
{
    needResize = false;	// nothing here depends on the window size

    // the probe count decides the atlas size, settle it before the graph records anything this frame
    LightProbeComponent* probeGrid = findProbeGrid();
    if(probeGrid && probeGrid->spacing > 0.0f) {
        probeCounts = gridProbeCounts(*probeGrid, MAX_PROBES_PER_DIMENSION);
    }

    if(_createResources()) {
        for(int i = 0; i < VulkanUtils::numFrames(); i++) {
            _updateDescriptor(i);
        }
    }

    RenderPassNode node{};
    node.name = "DDGI";
    node.reads = {};
    
    node.writes = {
        { rayColorHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { rayDistanceHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { currentIrradianceHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { lastIrradianceHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { currentVisibilityHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { lastVisibilityHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
    };

    node.execute = [this] (VkCommandBuffer cmd, uint32_t currentFrame){
        Scene* scene = SceneManager::getInstance().getActiveScene();
        Camera* camera = SceneManager::cameraController;

        if(!camera) {
            return;
        }

        render(*camera);
    };

    graph.addPass(node);
}
