#include "TonemapPassVulkan.h"
#include "DLSSPassVulkan.h"
#include "DeferredCombinePassVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/camera.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>

TonemapPassVulkan::TonemapPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{
    setFootprint("TonemapPassVulkan", sizeof(TonemapPassVulkan), MemorySystem::Graphics);
}

TonemapPassVulkan::~TonemapPassVulkan()
{
}

bool TonemapPassVulkan::init(WindowConfig config)
{
    PostProcessRendererVulkan::init(config);

    settings = &rendererManagerVulkan->getGraphicsSettings().add<TonemapSettings>("Tonemap");
    _fetchInputImage();
    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool TonemapPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();
    pipeline->destroy();
    rendererManagerVulkan->getGraphicsSettings().remove<TonemapSettings>();
    settings = nullptr;
    return true;
}

void TonemapPassVulkan::onUpdate()
{
    PostProcessRendererVulkan::onUpdate();
}

void TonemapPassVulkan::_fetchInputImage()
{
    if(rendererManagerVulkan->isUpscaling()) {
        inputImage = dynamic_cast<DLSSPassVulkan*>(rendererManagerVulkan->getRenderer("DLSSPassVulkan"))->getOutputImage();
    } else {
        inputImage = dynamic_cast<DeferredCombinePassVulkan*>(rendererManagerVulkan->getRenderer("DeferredCombinePassVulkan"))->getOutputImage();
    }
}

void TonemapPassVulkan::_createResources()
{
    RGTextureDesc desc{};
    desc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.outputSized = true;

    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
    uint32_t previousOutputID = outputImageID;
    outputHandle = graph.createRenderTexture("Tonemap.Output", desc);
    outputImage = graph.getTexture(outputHandle);
    outputImageID = outputImage->id();

    // compared by id, a recreated texture can land at the same address as the one it replaced
    if(outputImageID != previousOutputID) {
        // ApplicationRendererVulkan draws the display image through the bindless set when the gui is hidden,
        // the debug pass also reads the scene through it
        textureManagerVulkan->registerBindlessTexture(outputImageID);
    }
}

void TonemapPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(layoutID);

	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline("assets/shaders/spv/tonemap.comp.spv", { layout }, sizeof(PushConstant));
}

void TonemapPassVulkan::_createDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

	uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount },
	};

    layoutID = descriptorManagerVulkan->createLayout(bindings);
    poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);

    for(uint32_t i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void TonemapPassVulkan::_updateDescriptor(uint32_t index)
{
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

    VkDescriptorImageInfo inputInfo{};
	inputInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputInfo.imageView = inputImage->textureImageView;
	inputInfo.sampler = inputImage->textureSampler;

    VkDescriptorImageInfo outputInfo{};
	outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputInfo.imageView = outputImage->textureImageView;
	outputInfo.sampler = outputImage->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 0, inputInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 1, outputInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void TonemapPassVulkan::writeTonemap(VkCommandBuffer cmd, uint32_t currentFrame)
{
    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(setsID)[currentFrame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    tonemapPush.tonemapper = std::clamp(settings->tonemapper, 0, 6);
    tonemapPush.exposure = std::max(settings->exposure, 0.0f);
    tonemapPush.invGamma = 1.0f / std::max(settings->gamma, 0.1f);
    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &tonemapPush);

    uint32_t groupX = (outputImage->width() + 15) / 16;
    uint32_t groupY = (outputImage->height() + 15) / 16;
    vkCmdDispatch(cmd, groupX, groupY, 1);
}

void TonemapPassVulkan::render(Camera& camera)
{
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    _updateDescriptor(currentFrame);

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "Tonemap Pass");
    writeTonemap(cmd, currentFrame);
	renderDeviceVulkan->endLabel(cmd);

    rendererManagerVulkan->setDisplayImage(outputImage);
}

void TonemapPassVulkan::_recreateResources()
{
}

void TonemapPassVulkan::_cleanupResources()
{
}

void TonemapPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    needResize = false;     // the graph resizes the output, the input is looked up every frame
    _fetchInputImage();
    _createResources();

    auto input = graph.importTexture(inputImage);

    RenderPassNode node{};
    node.settings = settings;
    node.name = "Tonemap";
    node.reads = { { input, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };
    node.writes = { { outputHandle, VK_IMAGE_LAYOUT_GENERAL } };
    node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        Camera* camera = SceneManager::cameraController;
        if(!camera) {
            return;
        }
        render(*camera);
    };
    graph.addPass(node);
}
