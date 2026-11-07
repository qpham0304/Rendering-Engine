#include "BlurPassVulkan.h"
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>

BlurPassVulkan::BlurPassVulkan(std::string serviceName)
	:	RendererVulkan(serviceName)
{
	setFootprint("BlurPassVulkan", sizeof(BlurPassVulkan), MemorySystem::Graphics);
}

BlurPassVulkan::~BlurPassVulkan()
{
}

bool BlurPassVulkan::init(WindowConfig config)
{
    RendererVulkan::init(config);

    layoutID = descriptorManagerVulkan->createLayout({
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
    });
    _createPipelines();

    return true;
}

bool BlurPassVulkan::onClose()
{
    RendererVulkan::onClose();
    _cleanupResources();
    return true;
}

void BlurPassVulkan::onUpdate()
{
}

void BlurPassVulkan::render(Camera& camera)
{
}

// nothing of its own to draw, the passes that need a blur add it to the graph through blur()
void BlurPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    needResize = false;
}

RGHandle BlurPassVulkan::blur(RenderGraphVulkan& graph, const std::string& name, RGHandle input, const BlurParams& params)
{
    TextureVulkan* inputImage = graph.getTexture(input);

    // fixed to the input's size and format, so a half res or rgba input gets a matching blur
    RGTextureDesc desc{};
    desc.config.width = inputImage->width();
    desc.config.height = inputImage->height();
    desc.config.format = inputImage->format;
    desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    RGHandle temp = graph.createRenderTexture(name + ".Blur.Temp", desc);
    RGHandle output = graph.createRenderTexture(name + ".Blur.Output", desc);

    // bilateral without a depth image falls back to a plain gaussian
    bool bilateral = params.type == BlurType::Bilateral && params.depth.has_value();
    BlurType type = params.type == BlurType::Bilateral && !bilateral ? BlurType::Gaussian : params.type;
    RGHandle depth = bilateral ? *params.depth : input;
    TextureVulkan* depthImage = graph.getTexture(depth);
    TextureVulkan* tempImage = graph.getTexture(temp);
    TextureVulkan* outputImage = graph.getTexture(output);

    // compared by id, a recreated texture can land at the same address
    Request& request = _getRequest(name);
    if(request.inputID != inputImage->id() || request.depthID != depthImage->id()
        || request.tempID != tempImage->id() || request.outputID != outputImage->id()) {
        _updateDescriptors(request, inputImage, depthImage, tempImage, outputImage);
    }
    request.push.invProj = params.invProj;
    request.push.type = static_cast<int>(type);
    request.push.radius = params.radius;
    request.push.sigma = params.sigma;
    request.push.depthScale = params.depthScale;

    RenderPassNode horizontal{};
    horizontal.name = name + ".BlurH";
    horizontal.reads = { { input, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };
    if(bilateral) {
        horizontal.reads.push_back({ depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    }
    horizontal.writes = { { temp, VK_IMAGE_LAYOUT_GENERAL } };
    horizontal.execute = [this, &request, tempImage](VkCommandBuffer cmd, uint32_t currentFrame) {
        _writeBlur(cmd, request, request.setsHorizontalID, tempImage, false, currentFrame);
    };
    graph.addPass(horizontal);

    RenderPassNode vertical{};
    vertical.name = name + ".BlurV";
    vertical.reads = { { temp, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };
    if(bilateral) {
        vertical.reads.push_back({ depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    }
    vertical.writes = { { output, VK_IMAGE_LAYOUT_GENERAL } };
    vertical.execute = [this, &request, outputImage](VkCommandBuffer cmd, uint32_t currentFrame) {
        _writeBlur(cmd, request, request.setsVerticalID, outputImage, true, currentFrame);
    };
    graph.addPass(vertical);

    return output;
}

// one pool and a horizontal + vertical set per frame for every pass that asks for a blur
BlurPassVulkan::Request& BlurPassVulkan::_getRequest(const std::string& name)
{
    auto it = m_requests.find(name);
    if(it != m_requests.end()) {
        return it->second;
    }

    uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 4 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 2 },
    };

    Request request{};
    request.poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount * 2);
    request.setsHorizontalID = descriptorManagerVulkan->createSets(layoutID, request.poolID, frameCount);
    request.setsVerticalID = descriptorManagerVulkan->createSets(layoutID, request.poolID, frameCount);
    return m_requests.emplace(name, request).first->second;
}

void BlurPassVulkan::_updateDescriptors(Request& request, TextureVulkan* input, TextureVulkan* depth, TextureVulkan* temp, TextureVulkan* output)
{
    renderDeviceVulkan->waitIdle();     // every frame's sets are rewritten, an older frame may still read them

    auto horizontalSets = descriptorManagerVulkan->getDescriptorSet(request.setsHorizontalID);
    auto verticalSets = descriptorManagerVulkan->getDescriptorSet(request.setsVerticalID);

    VkDescriptorImageInfo inputInfo{ input->textureSampler, input->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo depthInfo{ depth->textureSampler, depth->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo tempReadInfo{ temp->textureSampler, temp->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo tempWriteInfo{ temp->textureSampler, temp->textureImageView, VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo outputInfo{ output->textureSampler, output->textureImageView, VK_IMAGE_LAYOUT_GENERAL };

    std::vector<VkWriteDescriptorSet> writes;
    for(uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
        descriptorManagerVulkan->writeImage(&writes, horizontalSets[i], 0, inputInfo);
        descriptorManagerVulkan->writeImage(&writes, horizontalSets[i], 1, depthInfo);
        descriptorManagerVulkan->writeStorageImage(&writes, horizontalSets[i], 2, tempWriteInfo);

        descriptorManagerVulkan->writeImage(&writes, verticalSets[i], 0, tempReadInfo);
        descriptorManagerVulkan->writeImage(&writes, verticalSets[i], 1, depthInfo);
        descriptorManagerVulkan->writeStorageImage(&writes, verticalSets[i], 2, outputInfo);
    }
    descriptorManagerVulkan->updateDescriptorSets(&writes);

    request.inputID = input->id();
    request.depthID = depth->id();
    request.tempID = temp->id();
    request.outputID = output->id();
}

void BlurPassVulkan::_writeBlur(VkCommandBuffer cmd, const Request& request, uint32_t setsID, TextureVulkan* target, bool vertical, uint32_t currentFrame)
{
    PushConstant push = request.push;
    push.vertical = vertical ? 1 : 0;

    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &push);
    auto sets = descriptorManagerVulkan->getDescriptorSet(setsID);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipelineLayout, 0, 1, &sets[currentFrame], 0, nullptr);

    uint32_t groupX = (target->width() + 15) / 16;
    uint32_t groupY = (target->height() + 15) / 16;
    vkCmdDispatch(cmd, groupX, groupY, 1);
}

void BlurPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	pipeline->createComputePipeline(
		"assets/shaders/spv/blur.comp.spv",
		{ layout },
		sizeof(PushConstant)
	);
}

void BlurPassVulkan::_recreateResources()
{
}

void BlurPassVulkan::_cleanupResources()
{
    pipeline->destroy();
}
