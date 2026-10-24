#include "SkyAtmosphereVulkan.h"
#include "ImageBasedVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/Camera.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <cstring>

namespace {
	// earth, from the paper (per km)
	const glm::vec3 RAYLEIGH_SCATTERING = glm::vec3(5.802f, 13.558f, 33.1f) * 1e-3f;
	const float RAYLEIGH_SCALE_HEIGHT = 8.0f;
	const float MIE_SCATTERING = 3.996e-3f;
	const float MIE_EXTINCTION = 4.440e-3f;
	const float MIE_SCALE_HEIGHT = 1.2f;
	const glm::vec3 OZONE_ABSORPTION = glm::vec3(0.650f, 1.881f, 0.085f) * 1e-3f;
	const float OZONE_PEAK_ALTITUDE = 25.0f;
	const float OZONE_HALF_WIDTH = 15.0f;
	const float BOTTOM_RADIUS = 6360.0f;
	const float TOP_RADIUS = 6460.0f;

	const uint32_t TRANSMITTANCE_WIDTH = 256;
	const uint32_t TRANSMITTANCE_HEIGHT = 64;
	const uint32_t MULTI_SCATTERING_SIZE = 32;
	const uint32_t ENVIRONMENT_WIDTH = 512;
	const uint32_t ENVIRONMENT_HEIGHT = 256;

	// the part of the params the luts depend on, everything before the sun
	const size_t LUT_PARAMS_SIZE = offsetof(SkyAtmosphereVulkan::AtmosphereParams, sunDirection);
}

SkyAtmosphereVulkan::SkyAtmosphereVulkan(std::string serviceName)
	: RendererVulkan(serviceName)
{
	setFootprint("SkyAtmosphereVulkan", sizeof(SkyAtmosphereVulkan), MemorySystem::Graphics);

}

SkyAtmosphereVulkan::~SkyAtmosphereVulkan()
{

}

bool SkyAtmosphereVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	settings = &rendererManagerVulkan->getGraphicsSettings().add<SkyAtmosphereSettings>("Sky Atmosphere");

	_createResources();
	_createDescriptors();
	_createPipelines();

	return true;
}

bool SkyAtmosphereVulkan::onClose()
{
	rendererManagerVulkan->getGraphicsSettings().remove<SkyAtmosphereSettings>();
	settings = nullptr;

	_cleanupResources();

	return true;
}

void SkyAtmosphereVulkan::onUpdate()
{

}

void SkyAtmosphereVulkan::render(Camera& camera)
{
	// the work is split over three graph nodes, see addToGraph
}

void SkyAtmosphereVulkan::addToGraph(RenderGraphVulkan& graph)
{
	auto imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan"));

	if(!settings->enabled) {
		if(overrideActive) {
			imageBasedRenderer->setEnvironmentOverride(nullptr);	// back to the environment map
			overrideActive = false;
		}
		return;
	}

	_createResources();
	_updateParams();

	if(!overrideActive) {
		imageBasedRenderer->setEnvironmentOverride(environmentMap);
		overrideActive = true;
		environmentDirty = true;
	}

	// fixed size, only rebuilt once unless the atmosphere changes
	if(lutDirty) {
		RenderPassNode transmittanceNode{};
		transmittanceNode.name = "Sky.Transmittance";
		transmittanceNode.writes = { { transmittanceHandle, VK_IMAGE_LAYOUT_GENERAL } };
		transmittanceNode.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
			_dispatch(cmd, transmittancePipeline.get(), transmittanceSetID, transmittanceLut);
		};
		graph.addPass(transmittanceNode);

		RenderPassNode multiScatteringNode{};
		multiScatteringNode.name = "Sky.MultiScattering";
		multiScatteringNode.reads = { { transmittanceHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };
		multiScatteringNode.writes = { { multiScatteringHandle, VK_IMAGE_LAYOUT_GENERAL } };
		multiScatteringNode.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
			_dispatch(cmd, multiScatteringPipeline.get(), multiScatteringSetID, multiScatteringLut);
		};
		graph.addPass(multiScatteringNode);
	}

	// the sun moved or the settings changed, the ibl rebuilds its sh and prefilter from the new sky this frame
	if(environmentDirty) {
		RenderPassNode environmentNode{};
		environmentNode.name = "Sky.Environment";
		environmentNode.reads = {
			{ transmittanceHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ multiScatteringHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		};
		environmentNode.writes = { { environmentHandle, VK_IMAGE_LAYOUT_GENERAL } };
		environmentNode.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
			_dispatch(cmd, environmentPipeline.get(), environmentSetID, environmentMap);
		};
		graph.addPass(environmentNode);

		imageBasedRenderer->requestUpdate();
	}

	lutDirty = false;
	environmentDirty = false;
}

void SkyAtmosphereVulkan::_updateParams()
{
	const GraphicsSettingsSerializer& graphicsSettings = rendererManagerVulkan->getGraphicsSettings();
	const ShadowMapSettings* shadowSettings = graphicsSettings.get<ShadowMapSettings>();
	const LightingSettings* lightingSettings = graphicsSettings.get<LightingSettings>();

	// same sun the light pass and the shadow map use
	glm::vec3 sunDirection = shadowSettings ? shadowSettings->lightDir : glm::vec3(0.0f, 1.0f, 0.0f);
	if(glm::length(sunDirection) < 0.0001f) {
		sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
	}
	glm::vec4 sunColor = lightingSettings ? lightingSettings->sunColor * lightingSettings->sunIntensity : glm::vec4(1.0f);

	params.rayleighScattering = glm::vec4(RAYLEIGH_SCATTERING * settings->rayleighScale, RAYLEIGH_SCALE_HEIGHT);
	params.mie = glm::vec4(MIE_SCATTERING * settings->mieScale, MIE_EXTINCTION * settings->mieScale, MIE_SCALE_HEIGHT, settings->mieAnisotropy);
	params.ozoneAbsorption = glm::vec4(settings->ozone ? OZONE_ABSORPTION : glm::vec3(0.0f), OZONE_PEAK_ALTITUDE);
	params.groundAlbedo = glm::vec4(glm::vec3(settings->groundAlbedo), OZONE_HALF_WIDTH);
	params.sunDirection = glm::vec4(glm::normalize(sunDirection), 0.0f);
	params.sunIlluminance = glm::vec4(glm::vec3(sunColor) * settings->intensity, 0.0f);
	params.bottomRadius = BOTTOM_RADIUS;
	params.topRadius = TOP_RADIUS;
	params.viewHeight = std::max(settings->viewHeight, 0.001f);	// exactly on the ground the horizon ray grazes the planet
	params.padding = 0.0f;

	if(std::memcmp(&params, &lastParams, LUT_PARAMS_SIZE) != 0) {
		lutDirty = true;
	}
	if(std::memcmp(&params, &lastParams, sizeof(AtmosphereParams)) != 0) {
		environmentDirty = true;
	}
	lastParams = params;
}

void SkyAtmosphereVulkan::_dispatch(VkCommandBuffer cmd, VulkanPipeline* pipeline, uint32_t setID, TextureVulkan* target)
{
	pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

	VkDescriptorSet descriptorSet = descriptorManagerVulkan->getDescriptorSet(setID)[0];
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(AtmosphereParams), &params);

	vkCmdDispatch(cmd, (target->width() + 7) / 8, (target->height() + 7) / 8, 1);
}


#pragma region setup
void SkyAtmosphereVulkan::_createResources()
{
	RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
	auto createTexture = [&](const char* name, uint32_t width, uint32_t height, RGHandle& handle) {
		RGTextureDesc desc{};
		desc.config.width = width;
		desc.config.height = height;
		desc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;	// usage defaults to storage | sampled, kept between frames
		handle = graph.createRenderTexture(name, desc);
		return graph.getTexture(handle);
	};

	transmittanceLut = createTexture("Sky.Transmittance", TRANSMITTANCE_WIDTH, TRANSMITTANCE_HEIGHT, transmittanceHandle);
	multiScatteringLut = createTexture("Sky.MultiScattering", MULTI_SCATTERING_SIZE, MULTI_SCATTERING_SIZE, multiScatteringHandle);
	environmentMap = createTexture("Sky.Environment", ENVIRONMENT_WIDTH, ENVIRONMENT_HEIGHT, environmentHandle);

	// compared by id, a recreated texture can land at the same address
	bool recreated = transmittanceLut->id() != transmittanceID
		|| multiScatteringLut->id() != multiScatteringID
		|| environmentMap->id() != environmentID;
	transmittanceID = transmittanceLut->id();
	multiScatteringID = multiScatteringLut->id();
	environmentID = environmentMap->id();

	settings->transmittance = transmittanceID;
	settings->multiScattering = multiScatteringID;
	settings->environment = environmentID;

	if(recreated) {
		lutDirty = true;
		environmentDirty = true;
		overrideActive = false;		// the ibl has to point at the new environment
		if(transmittancePipeline) {
			_updateDescriptors();
		}
	}
}

void SkyAtmosphereVulkan::_createDescriptors()
{
	transmittanceLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	});
	multiScatteringLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	});
	environmentLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	});

	// single copies, the textures are only written on the frames the sky changes
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
	};
	descriptorPoolID = descriptorManagerVulkan->createPool(poolSizes, 3);
	transmittanceSetID = descriptorManagerVulkan->createSets(transmittanceLayoutID, descriptorPoolID, 1);
	multiScatteringSetID = descriptorManagerVulkan->createSets(multiScatteringLayoutID, descriptorPoolID, 1);
	environmentSetID = descriptorManagerVulkan->createSets(environmentLayoutID, descriptorPoolID, 1);

	_updateDescriptors();
}

void SkyAtmosphereVulkan::_updateDescriptors()
{
	VkDescriptorImageInfo transmittanceStorage{ transmittanceLut->textureSampler, transmittanceLut->textureImageView, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo transmittanceSampled{ transmittanceLut->textureSampler, transmittanceLut->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo multiScatteringStorage{ multiScatteringLut->textureSampler, multiScatteringLut->textureImageView, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo multiScatteringSampled{ multiScatteringLut->textureSampler, multiScatteringLut->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo environmentStorage{ environmentMap->textureSampler, environmentMap->textureImageView, VK_IMAGE_LAYOUT_GENERAL };

	VkDescriptorSet transmittanceSet = descriptorManagerVulkan->getDescriptorSet(transmittanceSetID)[0];
	VkDescriptorSet multiScatteringSet = descriptorManagerVulkan->getDescriptorSet(multiScatteringSetID)[0];
	VkDescriptorSet environmentSet = descriptorManagerVulkan->getDescriptorSet(environmentSetID)[0];

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, transmittanceSet, 0, transmittanceStorage);
	descriptorManagerVulkan->writeImage(&writes, multiScatteringSet, 0, transmittanceSampled);
	descriptorManagerVulkan->writeStorageImage(&writes, multiScatteringSet, 1, multiScatteringStorage);
	descriptorManagerVulkan->writeImage(&writes, environmentSet, 0, transmittanceSampled);
	descriptorManagerVulkan->writeImage(&writes, environmentSet, 1, multiScatteringSampled);
	descriptorManagerVulkan->writeStorageImage(&writes, environmentSet, 2, environmentStorage);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void SkyAtmosphereVulkan::_createPipelines()
{
	auto createPipeline = [&](const char* path, uint32_t layoutID) {
		auto pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
		pipeline->createComputePipeline(path, { descriptorManagerVulkan->getDescriptorLayout(layoutID) }, sizeof(AtmosphereParams));
		return pipeline;
	};

	transmittancePipeline = createPipeline("assets/shaders/spv/skyTransmittance.comp.spv", transmittanceLayoutID);
	multiScatteringPipeline = createPipeline("assets/shaders/spv/skyMultiScattering.comp.spv", multiScatteringLayoutID);
	environmentPipeline = createPipeline("assets/shaders/spv/skyEnvironment.comp.spv", environmentLayoutID);
}

void SkyAtmosphereVulkan::_recreateResources()
{
	
}

void SkyAtmosphereVulkan::_cleanupResources()
{
	transmittancePipeline->destroy();
	multiScatteringPipeline->destroy();
	environmentPipeline->destroy();
}
#pragma endregion setup
