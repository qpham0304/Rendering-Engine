#include "SSRGIPassVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/camera.h>
#include <graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpiplines/RayTraceRendererVulkan.h>
#include <graphics/framework/vulkan/renderers/features/ImageBasedVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/AccelStructureBufferVulkan.h>
#include <window/AppWindow.h>
#include <math.h>
#include <algorithm>

SSRGIPassVulkan::SSRGIPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{
    setFootprint("SSRGIPassVulkan", sizeof(SSRGIPassVulkan), MemorySystem::Graphics);

}

SSRGIPassVulkan::~SSRGIPassVulkan()
{

}

bool SSRGIPassVulkan::init(WindowConfig config)
{
    PostProcessRendererVulkan::init(config);
        
    RendererVulkan* renderer = nullptr;
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	auto lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImageHiZ = highZRendererVulkan->hizImage;
    depthImageRaw = geometryPass->renderTarget.depthTexture;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    colorImage = lightingPass->renderTarget.colorTexture;
    albedoImage = geometryPass->renderTarget.gBufferAlbedo;
    pbrImage = geometryPass->renderTarget.gPBR;
    emissiveImage = geometryPass->renderTarget.gBufferEmissive;

    bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));
    settings = &rendererManagerVulkan->getGraphicsSettings().add<SSRSettings>("Screen Space Reflections");
    rtSupported = rendererManagerVulkan->getRenderer("RayTraceRendererVulkan") != nullptr;

    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool SSRGIPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    rendererManagerVulkan->getGraphicsSettings().remove<SSRSettings>();
    settings = nullptr;

    pipeline->destroy();
    if(rayQueryPipeline) {
        rayQueryPipeline->destroy();
    }

    return true;
}

void SSRGIPassVulkan::onUpdate()
{
    PostProcessRendererVulkan::onUpdate();

}

void SSRGIPassVulkan::render(Camera &camera)
{
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    
    RendererVulkan* renderer = nullptr;
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	auto lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(renderer);
	
    depthImageHiZ = highZRendererVulkan->hizImage;
    depthImageRaw = geometryPass->renderTarget.depthTexture;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    colorImage = lightingPass->renderTarget.colorTexture;
    albedoImage = geometryPass->renderTarget.gBufferAlbedo;
    pbrImage = geometryPass->renderTarget.gPBR;
    emissiveImage = geometryPass->renderTarget.gBufferEmissive;

    ubo.view = camera.getViewMatrix();
    ubo.projection = camera.getProjectionMatrix();
    ubo.projection[1][1] *= -1.0f; 
    ubo.invProj = glm::inverse(ubo.projection);
    ubo.invView = glm::inverse(ubo.view);

    // same sun the light pass, the shadow map and ddgi use, the tlas fallback shades its hits with it
    const GraphicsSettingsSerializer& graphicsSettings = rendererManagerVulkan->getGraphicsSettings();
    const ShadowMapSettings* shadowSettings = graphicsSettings.get<ShadowMapSettings>();
    const LightingSettings* lightingSettings = graphicsSettings.get<LightingSettings>();
    const DDGISettings* ddgiSettings = graphicsSettings.get<DDGISettings>();
    glm::vec3 sunDirection = shadowSettings ? shadowSettings->lightDir : glm::vec3(0.0f, 1.0f, 0.0f);
    if(glm::length(sunDirection) < 0.0001f) {
        sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    ubo.sunDirection = glm::vec4(glm::normalize(sunDirection), 0.0f);
    ubo.sunColor = lightingSettings ? lightingSettings->sunColor * lightingSettings->sunIntensity : glm::vec4(0.0f);
    ubo.skyIntensity = ddgiSettings ? ddgiSettings->skyIntensity : 1.0f;	// traced hits see the same sky as the ddgi probes
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	pushConstant.screenRes = { traceWidth, traceHeight };
    pushConstant.maxDistance = settings->maxDistance;
    pushConstant.mostDetailedMip = std::clamp(settings->downsample, 0, 2);
    pushConstant.thickness = settings->thickness;
    pushConstant.time = AppWindow::getTime();
    pushConstant.frameSeed = rand() % 32768;
    pushConstant.maxSteps = settings->maxSteps;
    pushConstant.objectsRef = rendererManagerVulkan->getObjectsAddress();
    pushConstant.useTLAS = 0;

    // only switch to the ray query pipeline when it can actually trace, otherwise skip the tlas update entirely
    VulkanPipeline* activePipeline = pipeline.get();
    if(settings->useTLAS && rayQueryPipeline) {
        auto raytracer = dynamic_cast<RayTraceRendererVulkan*>(rendererManagerVulkan->getRenderer("RayTraceRendererVulkan"));
        raytracer->updateTlas();
        if(AccelStructureBufferVulkan* tlas = raytracer->m_rtBuilder.getTlas()) {
            _updateRayQueryDescriptor(currentFrame, tlas);
            activePipeline = rayQueryPipeline.get();
            pushConstant.useTLAS = 1;
        }
    }

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    renderDeviceVulkan->beginLabel(cmd, "SSRGI Pass");
    writeSSRGI(cmd, currentFrame, activePipeline);
    renderDeviceVulkan->endLabel(cmd);

}

void SSRGIPassVulkan::writeSSRGI(VkCommandBuffer cmd, uint32_t currentFrame, VulkanPipeline* activePipeline)
{
    activePipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    std::vector<VkDescriptorSet> sets = { descriptorManagerVulkan->getDescriptorSet(SSRGISetsID)[currentFrame] };
    if(rtSupported) {
        sets.push_back(descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0]);	// materials of the tlas hits
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        activePipeline->pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

    vkCmdPushConstants(cmd, activePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(PushConstant), &pushConstant);

    uint32_t groupX = (traceWidth  + 7) / 8;
    uint32_t groupY = (traceHeight + 7) / 8;

    vkCmdDispatch(cmd, groupX, groupY, 1);
}

void SSRGIPassVulkan::_createResources()
{
    // one graph owned output: nothing reads last frame's ssrgi, so it needs no per frame copies.
    // same texture every frame, the graph recreates it when the render size changes
    // traced at 1, 1/2 or 1/4 of the render size, the passes reading it sample with uvs so it upscales on read
    int downsample = settings ? std::clamp(settings->downsample, 0, 2) : 0;
    VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;

    RGTextureDesc desc{};
    desc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;	// usage defaults to storage | sampled
    desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.scale = 1.0f / static_cast<float>(1 << downsample);
    // same rounding as the graph so the dispatch covers the texture exactly
    traceWidth = std::max(1u, static_cast<uint32_t>(swapchain.swapChainExtent.width * desc.scale));
    traceHeight = std::max(1u, static_cast<uint32_t>(swapchain.swapChainExtent.height * desc.scale));

    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    outputHandle = graph.createRenderTexture("SSRGI.Output", desc);
    outputImage = graph.getTexture(outputHandle);
    outputImageID = outputImage->id();
}

void SSRGIPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(SSRGILayoutID);
    std::vector<VkDescriptorSetLayout> layouts = { layout };
    if(rtSupported) {
        layouts.push_back(descriptorManagerVulkan->getDescriptorLayout(textureManagerVulkan->getBindlessTextureLayout()));
    }

    // both share the layout, the plain one just never touches the tlas bindings
	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline(
		"assets/shaders/spv/SSRGI.comp.spv",
		layouts,
		sizeof(PushConstant)
	);

    if(rtSupported) {
        rayQueryPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
        rayQueryPipeline->createComputePipeline(
            "assets/shaders/spv/SSRGIRayQuery.comp.spv",
            layouts,
            sizeof(PushConstant)
        );
    }
}

void SSRGIPassVulkan::_createDescriptors()
{
    bindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

	uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 8},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
	};

    // tlas and sky sh for the ray query fallback, written every frame it is used
    if(rtSupported) {
        bindings.push_back({ 10, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });
        bindings.push_back({ 11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });
        poolSizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, frameCount });
        poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount });
    }

    SSRGILayoutID = descriptorManagerVulkan->createLayout(bindings);
    SSRGIPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
	SSRGISetsID = descriptorManagerVulkan->createSets(SSRGILayoutID, SSRGIPoolID, frameCount);

    for(int i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void SSRGIPassVulkan::_updateDescriptor(uint32_t index)
{
    RendererVulkan* renderer = nullptr;
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	auto lightingPass = dynamic_cast<LightingPassVulkan*>(rendererManagerVulkan->getRenderer("LightingPassVulkan"));
    renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(renderer);
	
    depthImageHiZ = highZRendererVulkan->hizImage;
    hizImageID = depthImageHiZ->id();
    depthImageRaw = geometryPass->renderTarget.depthTexture;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    colorImage = lightingPass->renderTarget.colorTexture;
    albedoImage = geometryPass->renderTarget.gBufferAlbedo;
    pbrImage = geometryPass->renderTarget.gPBR;
    emissiveImage = geometryPass->renderTarget.gBufferEmissive;

	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(SSRGISetsID);
    VkDescriptorImageInfo depthImageInfo{};
	depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthImageInfo.imageView = depthImageHiZ->textureImageView;
	depthImageInfo.sampler = depthImageHiZ->textureSampler;

    VkDescriptorImageInfo normalImageInfo{};
	normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalImageInfo.imageView = normalImage->textureImageView;
	normalImageInfo.sampler = normalImage->textureSampler;
    
    VkDescriptorImageInfo sceneColorImageInfo{};
	sceneColorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	sceneColorImageInfo.imageView = colorImage->textureImageView;
	sceneColorImageInfo.sampler = colorImage->textureSampler;

	VkDescriptorImageInfo outputImageInfo{};
	outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputImageInfo.imageView = outputImage->textureImageView;
	outputImageInfo.sampler = outputImage->textureSampler;

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[index]);
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo depthImageRawInfo{};
	depthImageRawInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthImageRawInfo.imageView = depthImageRaw->textureImageView;
	depthImageRawInfo.sampler = depthImageRaw->textureSampler;

    VkDescriptorImageInfo albedoImageInfo{};
	albedoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	albedoImageInfo.imageView = albedoImage->textureImageView;
	albedoImageInfo.sampler = albedoImage->textureSampler;

    VkDescriptorImageInfo pbrImageInfo{};
	pbrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	pbrImageInfo.imageView = pbrImage->textureImageView;
	pbrImageInfo.sampler = pbrImage->textureSampler;

    VkDescriptorImageInfo emissiveImageInfo{};
	emissiveImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	emissiveImageInfo.imageView = emissiveImage->textureImageView;
	emissiveImageInfo.sampler = emissiveImage->textureSampler;

    uint32_t imageID = textureManagerVulkan->loadTexture("assets/textures/obluenoise256.png", 1, true);
	TextureVulkan* blueNoiseImage = textureManagerVulkan->getTexture(imageID);
    VkDescriptorImageInfo blueNoiseInfo{};
	blueNoiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	blueNoiseInfo.imageView = blueNoiseImage->textureImageView;
	blueNoiseInfo.sampler = blueNoiseImage->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 0, depthImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 1, normalImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 2, sceneColorImageInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 3, outputImageInfo);
	descriptorManagerVulkan->writeUniform(&writes, descriptorSets[index], 4, bufferInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 5, depthImageRawInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 6, albedoImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 7, pbrImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 8, emissiveImageInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 9, blueNoiseInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void SSRGIPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();
    _createResources();
	_createDescriptors();
	_createPipelines();
}

void SSRGIPassVulkan::_updateRayQueryDescriptor(uint32_t index, AccelStructureBufferVulkan* tlas)
{
    // the tlas can be rebuilt and the environment reloaded between frames
    auto imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan"));
    const VkDescriptorBufferInfo& shInfo = imageBasedRenderer->finalSumBuffers[index]->getDescStorageBufferInfo();
    const VkWriteDescriptorSetAccelerationStructureKHR& tlasInfo = tlas->getDescAccelStructInfo();

	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(SSRGISetsID);
	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeAccelStruct(&writes, descriptorSets[index], 10, bindings, tlasInfo);
	descriptorManagerVulkan->writeStorage(&writes, descriptorSets[index], 11, shInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void SSRGIPassVulkan::_cleanupResources()
{
    pipeline->destroy();
    if(rayQueryPipeline) {
        rayQueryPipeline->destroy();
    }
}

void SSRGIPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    // same output as last frame unless the render size or downsample changed, then the graph recreated it
    uint32_t previousOutputID = outputImageID;
    _createResources();

    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    // compared by id, a recreated texture can land at the same address
    auto highZRendererVulkan = dynamic_cast<HiZPassVulkan*>(rendererManagerVulkan->getRenderer("HiZPassVulkan"));
    if(outputImageID != previousOutputID || highZRendererVulkan->hizImage->id() != hizImageID) {
        for(uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
            _updateDescriptor(i);
        }
    }

   
    auto depthImageHiZHandle = graph.importTexture(depthImageHiZ);
    auto depthImageRawHandle = graph.importTexture(depthImageRaw);
    auto normalImageHandle = graph.importTexture(normalImage);
    auto colorImageHandle = graph.importTexture(colorImage);
    auto albedoImageHandle = graph.importTexture(albedoImage);
    auto pbrImageHandle = graph.importTexture(pbrImage);
    auto emissiveImageHandle = graph.importTexture(emissiveImage);

    RenderPassNode node{};
    node.name = "SSRGI";
    node.reads = {
        { depthImageHiZHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { depthImageRawHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { normalImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { colorImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { albedoImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { pbrImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        { emissiveImageHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
    };
    
    node.writes = {
        { outputHandle, VK_IMAGE_LAYOUT_GENERAL }
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
