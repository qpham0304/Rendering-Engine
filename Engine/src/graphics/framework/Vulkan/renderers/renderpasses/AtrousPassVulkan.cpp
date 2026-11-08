#include "AtrousPassVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/camera.h>
#include <graphics/framework/vulkan/renderers/renderpasses/TemporalPassVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/GeometryPassVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <algorithm>

namespace {
    const char* kPassName = "A-Trous";
}

AtrousPassVulkan::AtrousPassVulkan(std::string serviceName)
    : RendererVulkan(serviceName)
{
    setFootprint("AtrousPassVulkan", sizeof(AtrousPassVulkan), MemorySystem::Graphics);
}

AtrousPassVulkan::~AtrousPassVulkan()
{
}

bool AtrousPassVulkan::init(WindowConfig config)
{
    RendererVulkan::init(config);

    settings = &rendererManagerVulkan->getGraphicsSettings().add<AtrousSettings>("A-Trous Denoise");

    _createTextures(rendererManagerVulkan->getRenderGraph());
    _createDescriptors();

    pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline(
        "assets/shaders/spv/atrous.comp.spv",
        { descriptorManagerVulkan->getDescriptorLayout(layoutID) },
        sizeof(PushConstant)
    );

    return true;
}

bool AtrousPassVulkan::onClose()
{
    pipeline->destroy();
    rendererManagerVulkan->getGraphicsSettings().remove<AtrousSettings>();
    settings = nullptr;
    return true;
}

bool AtrousPassVulkan::isFiltering() const
{
    return activeFrame == rendererManagerVulkan->getFrameNumber()
        && rendererManagerVulkan->getRenderGraph().isPassEnabled(kPassName);
}

// iteration i writes texture i % 2, so the last one written holds the result
TextureVulkan* AtrousPassVulkan::getDiffuseOutputImage() const
{
    return diffuse[(std::max(iterations, 1) - 1) % 2];
}

TextureVulkan* AtrousPassVulkan::getSpecularOutputImage() const
{
    return specular[(std::max(iterations, 1) - 1) % 2];
}

void AtrousPassVulkan::_createTextures(RenderGraphVulkan& graph)
{
    // graph owned, the same textures every frame until the render size changes
    auto createTexture = [&](const char* name, RGHandle& handle) {
        RGTextureDesc desc{};
        desc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        desc.config.aspectBits = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.config.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        handle = graph.createRenderTexture(name, desc);
        return graph.getTexture(handle);
    };

    diffuse[0] = createTexture("Atrous.DiffuseA", diffuseHandles[0]);
    diffuse[1] = createTexture("Atrous.DiffuseB", diffuseHandles[1]);
    specular[0] = createTexture("Atrous.SpecularA", specularHandles[0]);
    specular[1] = createTexture("Atrous.SpecularB", specularHandles[1]);
}

void AtrousPassVulkan::_createDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

    uint32_t frameCount = VulkanUtils::numFrames();
    uint32_t setCount = frameCount * 3;
    std::vector<VkDescriptorPoolSize> poolSizes {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * setCount },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6 * setCount },
    };

    layoutID = descriptorManagerVulkan->createLayout(bindings);
    poolID = descriptorManagerVulkan->createPool(poolSizes, setCount);
    setsFromInputID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);
    setsAToBID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);
    setsBToAID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);
}

// the inputs flip with the temporal ping-pong and the graph textures can be recreated, so all three sets of this
// frame are written again every frame (after its fence, the other frame's sets may still be in use)
void AtrousPassVulkan::_updateDescriptors(uint32_t frameIndex)
{
    auto sampled = [](TextureVulkan* texture, VkImageLayout layout) {
        return VkDescriptorImageInfo{ texture->textureSampler, texture->textureImageView, layout };
    };
    const VkImageLayout readOnly = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const VkImageLayout general = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo depthInfo = sampled(depthImage, readOnly);
    VkDescriptorImageInfo normalInfo = sampled(normalImage, readOnly);
    VkDescriptorImageInfo pbrInfo = sampled(pbrImage, readOnly);
    VkDescriptorImageInfo lengthInfo = sampled(historyLength, readOnly);

    struct SetSources {
        uint32_t setsID;
        VkDescriptorImageInfo inDiffuse;
        VkDescriptorImageInfo inSpecular;
        VkDescriptorImageInfo outDiffuse;
        VkDescriptorImageInfo outSpecular;
    };
    SetSources sources[3] {
        { setsFromInputID, sampled(inputDiffuse, readOnly), sampled(inputSpecular, readOnly), sampled(diffuse[0], general), sampled(specular[0], general) },
        { setsAToBID, sampled(diffuse[0], general), sampled(specular[0], general), sampled(diffuse[1], general), sampled(specular[1], general) },
        { setsBToAID, sampled(diffuse[1], general), sampled(specular[1], general), sampled(diffuse[0], general), sampled(specular[0], general) },
    };

    std::vector<VkWriteDescriptorSet> writes;
    for(SetSources& source : sources) {
        const VkDescriptorSet& set = descriptorManagerVulkan->getDescriptorSet(source.setsID)[frameIndex];
        descriptorManagerVulkan->writeImage(&writes, set, 0, source.inDiffuse);
        descriptorManagerVulkan->writeImage(&writes, set, 1, source.inSpecular);
        descriptorManagerVulkan->writeStorageImage(&writes, set, 2, source.outDiffuse);
        descriptorManagerVulkan->writeStorageImage(&writes, set, 3, source.outSpecular);
        descriptorManagerVulkan->writeImage(&writes, set, 4, depthInfo);
        descriptorManagerVulkan->writeImage(&writes, set, 5, normalInfo);
        descriptorManagerVulkan->writeImage(&writes, set, 6, pbrInfo);
        descriptorManagerVulkan->writeImage(&writes, set, 7, lengthInfo);
    }
    descriptorManagerVulkan->updateDescriptorSets(&writes);
}

void AtrousPassVulkan::_execute(VkCommandBuffer cmd, uint32_t frameIndex)
{
    Camera* camera = SceneManager::cameraController;
    if(!camera) {
        return;
    }

    glm::mat4 proj = camera->getProjectionMatrix();
    proj[1][1] *= -1.0f;
    pushConstant.invProj = glm::inverse(proj);
    pushConstant.screenRes = { static_cast<float>(renderWidth), static_cast<float>(renderHeight) };
    pushConstant.depthSigma = std::max(settings->depthSigma, 0.0001f);
    pushConstant.normalPower = std::max(settings->normalPower, 1.0f);
    pushConstant.luminanceSigma = std::max(settings->luminanceSigma, 0.01f);
    pushConstant.specularRoughnessStart = std::clamp(settings->specularRoughnessStart, 0.0f, 1.0f);

    uint32_t groupX = (static_cast<uint32_t>(pushConstant.screenRes.x) + 15) / 16;
    uint32_t groupY = (static_cast<uint32_t>(pushConstant.screenRes.y) + 15) / 16;

    renderDeviceVulkan->beginLabel(cmd, "A-Trous Denoise");
    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    for(int i = 0; i < iterations; i++) {
        uint32_t setsID = i == 0 ? setsFromInputID : (i % 2 == 1 ? setsAToBID : setsBToAID);
        VkDescriptorSet set = descriptorManagerVulkan->getDescriptorSet(setsID)[frameIndex];
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipelineLayout, 0, 1, &set, 0, nullptr);

        pushConstant.stepSize = 1 << i;
        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &pushConstant);
        vkCmdDispatch(cmd, groupX, groupY, 1);

        // the next iteration samples what this one wrote, both stay in the general layout
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    renderDeviceVulkan->endLabel(cmd);
}

void AtrousPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    iterations = std::clamp(settings->iterations, 1, 5);
    if(!settings->enabled) {
        return;
    }

    auto temporalPass = dynamic_cast<TemporalPassVulkan*>(rendererManagerVulkan->getRenderer("TemporalPassVulkan"));
    auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    inputDiffuse = temporalPass->getOutputImage();
    inputSpecular = temporalPass->getSpecularOutputImage();
    historyLength = temporalPass->getHistoryLengthImage();
    depthImage = geometryPass->renderTarget.depthTexture;
    normalImage = geometryPass->renderTarget.gBufferNorm;
    pbrImage = geometryPass->renderTarget.gPBR;
    renderWidth = geometryPass->renderTarget.width;
    renderHeight = geometryPass->renderTarget.height;

    _createTextures(graph);
    _updateDescriptors(renderDeviceVulkan->getCurrentFrameIndex());
    activeFrame = rendererManagerVulkan->getFrameNumber();

    RenderPassNode node{};
    node.name = kPassName;
    node.settings = settings;
    node.reads = {
        { graph.importTexture(inputDiffuse), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { graph.importTexture(inputSpecular), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { graph.importTexture(historyLength), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { graph.importTexture(depthImage), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { graph.importTexture(normalImage), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { graph.importTexture(pbrImage), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
    };
    node.writes = {
        { diffuseHandles[0], VK_IMAGE_LAYOUT_GENERAL },
        { diffuseHandles[1], VK_IMAGE_LAYOUT_GENERAL },
        { specularHandles[0], VK_IMAGE_LAYOUT_GENERAL },
        { specularHandles[1], VK_IMAGE_LAYOUT_GENERAL },
    };
    node.execute = [this](VkCommandBuffer cmd, uint32_t frameIndex) {
        _execute(cmd, frameIndex);
    };

    graph.addPass(node);
}

void AtrousPassVulkan::_recreateResources()
{
}

void AtrousPassVulkan::_cleanupResources()
{
}
