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
#include <window/AppWindow.h>
#include <math.h>
#include <algorithm>

SSRGIPassVulkan::SSRGIPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{

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

    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool SSRGIPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    
    pipeline->destroy();
    
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
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	pushConstant.screenRes = { geometryPass->renderTarget.width, geometryPass->renderTarget.height };
    pushConstant.maxDistance = 500.0f;
    pushConstant.maxMip = 11;          
    pushConstant.thickness = 0.05f;
    pushConstant.time = AppWindow::getTime();
    pushConstant.frameSeed = rand() % 32768;

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    renderDeviceVulkan->beginLabel(cmd, "SSRGI Pass");
    writeSSRGI(cmd, currentFrame);
    renderDeviceVulkan->endLabel(cmd);

}

void SSRGIPassVulkan::writeSSRGI(VkCommandBuffer cmd, uint32_t currentFrame)
{
    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(SSRGISetsID)[currentFrame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
        pipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 
        0, sizeof(PushConstant), &pushConstant);

	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    uint32_t width = geometryPass->renderTarget.width;
    uint32_t height = geometryPass->renderTarget.height;
    uint32_t groupX = (width  + 7) / 8;
    uint32_t groupY = (height + 7) / 8;

    vkCmdDispatch(cmd, groupX, groupY, 1);
}

void SSRGIPassVulkan::_createResources()
{
    // one graph owned output: nothing reads last frame's ssrgi, so it needs no per frame copies.
    // same texture every frame, the graph recreates it when the render size changes
    RGTextureDesc desc{};
    desc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;	// usage defaults to storage | sampled
    desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    outputHandle = graph.createRenderTexture("SSRGI.Output", desc);
    outputImage = graph.getTexture(outputHandle);
    outputImageID = outputImage->id();
}

void SSRGIPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(SSRGILayoutID);
    
	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline(
		"assets/shaders/spv/SSRGI.comp.spv", 
		{ layout }, 
		sizeof(PushConstant)
	);
}

void SSRGIPassVulkan::_createDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings {
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

void SSRGIPassVulkan::_cleanupResources()
{
    pipeline->destroy();
}

void SSRGIPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    // same output as last frame unless the render size changed, then the graph recreated it
    _createResources();

    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
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
