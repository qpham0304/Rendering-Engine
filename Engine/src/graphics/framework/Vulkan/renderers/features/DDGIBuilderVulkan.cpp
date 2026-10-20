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
#include <imgui/imgui.h>
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
	renderer = rendererManagerVulkan->getRenderer("LightingPassVulkan");
	lightingPass = dynamic_cast<LightingPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
    assert(raytracer && shadowMapPass && lightingPass && imageBasedRenderer && "failed to retrieve renderer");

	ubo.hysteresis = 0.97f;
	ubo.normalBias = 0.3f;
	ubo.skyIntensity = 1.0f;

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

	needResize = false;	// nothing here depends on the window size

	LightProbeComponent* probeGrid = findProbeGrid();
	AccelStructureBufferVulkan* tlas = raytracer->m_rtBuilder.getTlas();
	hasProbe = probeGrid && probeGrid->spacing > 0.0f && tlas;
	if(!hasProbe || !enabled) {
		return;
	}

	glm::uvec3 newProbeCounts = gridProbeCounts(*probeGrid, MAX_PROBES_PER_DIMENSION);
	if(newProbeCounts != probeCounts) {
		probeCounts = newProbeCounts;
		_recreateResources();
	}

	// probes hold light for the place they were in, a moved grid has to start over
	if(probeGrid->gridOrigin != pushConstant.gridOrigin || probeGrid->spacing != pushConstant.gridSpacing) {
		resetHistory = true;
	}

	pushConstant.objectsRef = raytracer->objDeviceAddress;
	pushConstant.padding = 0;
	pushConstant.gridOrigin = probeGrid->gridOrigin;
	pushConstant.probeCounts = glm::ivec4(probeCounts, totalProbes);
	pushConstant.probesResolution = IRRADIANCE_RES;
	pushConstant.gridSpacing = probeGrid->spacing;

	glm::vec3 sunDirection = shadowMapPass->lightDir;
	if(glm::length(sunDirection) < 0.0001f) {
		sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
	}
	ubo.randomRotation = randomRotation();
	ubo.sunDirection = glm::vec4(glm::normalize(sunDirection), 0.0f);	// same direction and color the light pass and the shadow map use
	ubo.sunColor = lightingPass->sunColor * lightingPass->sunIntensity;
	ubo.maxRayDistance = probeGrid->spacing * 1.5f;	// a bit less than the diagonal of a grid cell
	ubo.clear = resetHistory ? 1 : 0;

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	// also refreshes raytracer->objects, which is what objectsRef points at
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

void DDGIBuilderVulkan::renderGui()
{
    ImGui::Begin("DDGI");
	ImGui::Checkbox("enabled", &enabled);
	ImGui::SameLine();
	ImGui::Checkbox("indirect only", &showIndirectOnly);
	ImGui::SameLine();
	if(ImGui::Button("reset probes")) {
		resetHistory = true;
	}
	ImGui::SliderFloat("intensity", &intensity, 0.0f, 5.0f);
	ImGui::SliderFloat("sky intensity", &ubo.skyIntensity, 0.0f, 2.0f);
	ImGui::SliderFloat("hysteresis", &ubo.hysteresis, 0.0f, 0.99f);
	ImGui::SliderFloat("normal bias", &ubo.normalBias, 0.0f, 1.0f);

	if(hasProbe) {
		ImGui::Text("%u x %u x %u probes, %u rays each", probeCounts.x, probeCounts.y, probeCounts.z, raysPerProbe);
		glm::vec3 gridMin = glm::vec3(pushConstant.gridOrigin);
		glm::vec3 gridMax = gridMin + glm::vec3(probeCounts - 1u) * pushConstant.gridSpacing;
		ImGui::Text("covers (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f)", gridMin.x, gridMin.y, gridMin.z, gridMax.x, gridMax.y, gridMax.z);
	} else {
		ImGui::Text("no light probe grid in the scene");
	}

	// the atlases are Cx tiles wide and Cy * Cz tiles tall
	float scale = 384.0f / static_cast<float>(visAtlasH);
	ImGui::Image(
		(ImTextureID)textureManagerVulkan->inspectTexture(lastframeIrradianceAtlas->id()),
		ImVec2(visAtlasW * scale, visAtlasH * scale)
	);
	ImGui::SameLine();
	ImGui::Image(
		(ImTextureID)textureManagerVulkan->inspectTexture(lastFrameVisibilityAtlas->id()),
		ImVec2(visAtlasW * scale, visAtlasH * scale)
	);
    ImGui::End();
}

TextureVulkan *DDGIBuilderVulkan::getAtlasImage()
{
    return lastframeIrradianceAtlas;
}

TextureVulkan *DDGIBuilderVulkan::getVisibilityAtlasImage()
{
    return lastFrameVisibilityAtlas;
}

bool DDGIBuilderVulkan::isReady() const
{
    return hasProbe && enabled;
}

glm::vec4 DDGIBuilderVulkan::getGridOrigin() const
{
    return pushConstant.gridOrigin;
}

float DDGIBuilderVulkan::getGridSpacing() const
{
    return pushConstant.gridSpacing;
}

glm::uvec3 DDGIBuilderVulkan::getProbeCounts() const
{
    return probeCounts;
}

float DDGIBuilderVulkan::getSkyIntensity() const
{
    return ubo.skyIntensity;
}

float DDGIBuilderVulkan::getNormalBias() const
{
    return ubo.normalBias;
}

float DDGIBuilderVulkan::getIntensity() const
{
    return intensity;
}

bool DDGIBuilderVulkan::getShowIndirectOnly() const
{
    return showIndirectOnly;
}

void DDGIBuilderVulkan::_createResources()
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

    auto createTexture = [&] (TextureVulkan*& texture, uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags extraUsage = 0) {
        TextureSamplerConfig samplerConfig = { VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR };
        TextureConfig imageConfig { .width = w, .height = h, .format = format};
		imageConfig.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | extraUsage;	// transfer dst for the clear below

        uint32_t id = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
        texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
    };

    // Width = Rays, Height = Total Probes
    createTexture(rayColorBuffer, rayBufferW, rayBufferH, VK_FORMAT_R16G16B16A16_SFLOAT);
    createTexture(rayDistanceBuffer, rayBufferW, rayBufferH, VK_FORMAT_R16G16_SFLOAT);
    createTexture(currentIrradianceAtlas, irrAtlasW, irrAtlasH, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    createTexture(lastframeIrradianceAtlas, irrAtlasW, irrAtlasH, VK_FORMAT_R16G16B16A16_SFLOAT);
    createTexture(currentVisibilityAtlas, visAtlasW, visAtlasH, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    createTexture(lastFrameVisibilityAtlas, visAtlasW, visAtlasH, VK_FORMAT_R16G16_SFLOAT);

	// new images hold garbage and the atlases feed back into themselves every frame
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

    rendererManagerVulkan->addRenderTexture("rayColorBuffer", {rayColorBuffer->id()});
    rendererManagerVulkan->addRenderTexture("rayDistanceBuffer", {rayDistanceBuffer->id()});
    rendererManagerVulkan->addRenderTexture("currentIrradianceAtlas", {currentIrradianceAtlas->id()});
    rendererManagerVulkan->addRenderTexture("lastframeIrradianceAtlas", {lastframeIrradianceAtlas->id()});
    rendererManagerVulkan->addRenderTexture("currentVisibilityAtlas", {currentVisibilityAtlas->id()});
    rendererManagerVulkan->addRenderTexture("lastFrameVisibilityAtlas", {lastFrameVisibilityAtlas->id()});

	resetHistory = true;
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
	textureManagerVulkan->destroy(rayColorBuffer->id());
	textureManagerVulkan->destroy(rayDistanceBuffer->id());
	textureManagerVulkan->destroy(currentIrradianceAtlas->id());
	textureManagerVulkan->destroy(lastframeIrradianceAtlas->id());
	textureManagerVulkan->destroy(currentVisibilityAtlas->id());
	textureManagerVulkan->destroy(lastFrameVisibilityAtlas->id());
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
    probTracePipeline->destroy();
    updateIrradiancePipeline->destroy();
    updateVisibilityPipeline->destroy();

    return true;
}

void DDGIBuilderVulkan::addToGraph(RenderGraphVulkan& graph)
{
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
   
    auto rayColorBufferHandle = graph.importTexture(rayColorBuffer);
    auto rayDistanceBufferHandle = graph.importTexture(rayDistanceBuffer);
    auto currentIrradianceAtlasHandle = graph.importTexture(currentIrradianceAtlas);
    auto lastframeIrradianceAtlasHandle = graph.importTexture(lastframeIrradianceAtlas);
    auto currentVisibilityAtlasHandle = graph.importTexture(currentVisibilityAtlas);
    auto lastFrameVisibilityAtlasHandle = graph.importTexture(lastFrameVisibilityAtlas);

    RenderPassNode node{};
    node.name = "DDGI";
    node.reads = {};
    
    node.writes = {
        { rayColorBufferHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { rayDistanceBufferHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { currentIrradianceAtlasHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { lastframeIrradianceAtlasHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { currentVisibilityAtlasHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { lastFrameVisibilityAtlasHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
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
