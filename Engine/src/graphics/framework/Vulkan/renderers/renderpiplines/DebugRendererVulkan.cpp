#include "DebugRendererVulkan.h"

#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include "core/features/ServiceLocator.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"

DebugRendererVulkan::DebugRendererVulkan(std::string serviceName)
    : RendererVulkan(serviceName)
{

}

DebugRendererVulkan::~DebugRendererVulkan()
{

}

bool DebugRendererVulkan::init(WindowConfig config)
{
    RendererVulkan::init(config);

    _createBuffers();
    _createResources();
    _createDescriptors();
    _updateDescriptors();
    _createPipelines();

    return true;
}

bool DebugRendererVulkan::onClose()
{
    return true;
}

void DebugRendererVulkan::onUpdate()
{

}

void DebugRendererVulkan::render(Camera &camera)
{
    if(needResize) {
		_recreateResources();
		needResize = false;
        _clearDrawRequests();
		return;
	}

    
    // if no draw data is empty/no one request draw debug, exit and keep the scene image untouched
    TextureVulkan* sceneImage = rendererManagerVulkan->getDisplayImage();
    bool hasOutlines = !m_outlineDraws.empty();
    if(!sceneImage || sceneImage == displayImage || (m_lineVertices.empty() && !hasOutlines)) {
        _clearDrawRequests();
        return;
    }

    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();

    uint32_t lineCount = std::min<uint32_t>(m_lineVertices.size(), MAX_DEBUG_VERTICES);
    m_vertexBuffers[currentFrame]->update(m_lineVertices.data(), lineCount * sizeof(DebugVertex));

    // hover first so a shape that is both hovered and selected keeps the select value
    std::stable_sort(m_outlineDraws.begin(), m_outlineDraws.end(), [](const OutlineDraw& a, const OutlineDraw& b) {
        return a.push.kind < b.push.kind;
    });

    pushConstant.imageIndex = sceneImage->id();
    combinePush.sceneIndex = sceneImage->id();
    combinePush.debugSceneIndex = debugImage->id();
    combinePush.maskIndex = maskImage->id();
    combinePush.hoverColor = hoverColor;
    combinePush.selectColor = selectColor;
    combinePush.hoverThickness = hoverThickness;
    combinePush.selectThickness = selectThickness;

    auto makeColorAttachment = [](TextureVulkan* image) {
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = image->textureImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = { {0.0f, 0.0f, 0.0f, 0.0f} };
        return colorAttachment;
    };

    VkRenderingAttachmentInfo colorAttachment = makeColorAttachment(debugImage);
    VkRenderingAttachmentInfo maskAttachment = makeColorAttachment(maskImage);

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {width, height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;

    auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
    auto debugSets = descriptorManagerVulkan->getDescriptorSet(setsID);
    VkDeviceSize offset = 0;

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    renderDeviceVulkan->beginLabel(cmd, "debug render");
    {
        debugImage->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        vkCmdBeginRendering(cmd, &renderingInfo);
        renderDeviceVulkan->setViewport(width, height);
        renderDeviceVulkan->setScissor(width, height);

        if(lineCount > 0) {
            VkBuffer vertexBuffer = static_cast<VkBuffer>(*m_vertexBuffers[currentFrame]);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, &debugSets[currentFrame], 0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 1, 1, &bindlessSet, 0, nullptr);
            vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DebugDrawPushConstant), &pushConstant);
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
            vkCmdDraw(cmd, lineCount, 1, 0, 0);
        }

        vkCmdEndRendering(cmd);
        debugImage->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    renderDeviceVulkan->endLabel(cmd);

    renderDeviceVulkan->beginLabel(cmd, "debug outline mask");
    {
        renderingInfo.pColorAttachments = &maskAttachment;

        maskImage->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        vkCmdBeginRendering(cmd, &renderingInfo);
        renderDeviceVulkan->setViewport(width, height);
        renderDeviceVulkan->setScissor(width, height);

        if(hasOutlines) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, maskPipeline->pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, maskPipeline->pipelineLayout, 0, 1, &debugSets[currentFrame], 0, nullptr);

            for(const OutlineDraw& draw : m_outlineDraws) {
                const OutlineMesh& mesh = m_outlineMeshes.at(draw.key);
                VkBuffer outlineBuffer = static_cast<VkBuffer>(*mesh.buffer);
                vkCmdBindVertexBuffers(cmd, 0, 1, &outlineBuffer, &offset);
                vkCmdPushConstants(cmd, maskPipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MaskPushConstant), &draw.push);
                vkCmdDraw(cmd, mesh.vertexCount, 1, 0, 0);
            }
        }

        vkCmdEndRendering(cmd);
        maskImage->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    renderDeviceVulkan->endLabel(cmd);

    renderDeviceVulkan->beginLabel(cmd, "debug render combine");
    {
        displayImage->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        auto displaySets = descriptorManagerVulkan->getDescriptorSet(displaySetsID);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, combinePipeline->pipeline);
        vkCmdPushConstants(cmd, combinePipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CombinePushConstant), &combinePush);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, combinePipeline->pipelineLayout, 0, 1, &displaySets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, combinePipeline->pipelineLayout, 1, 1, &bindlessSet, 0, nullptr);

        vkCmdDispatch(cmd, (width + 7) / 8, (height + 7) / 8, 1);
        displayImage->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    renderDeviceVulkan->endLabel(cmd);

	rendererManagerVulkan->setDisplayImage(displayImage);
    _clearDrawRequests();
}

void DebugRendererVulkan::drawLine(const glm::vec3 &start, const glm::vec3 &end)
 {
    m_lineVertices.push_back({start});
    m_lineVertices.push_back({end});
}

bool DebugRendererVulkan::hasOutlineMesh(uint32_t key) const
{
    return m_outlineMeshes.contains(key);
}

// outline geometry is uploaded once per key (collider shape id), only a transform is pushed each frame 
void DebugRendererVulkan::uploadOutlineMesh(uint32_t key, const std::vector<glm::vec3>& localTriangles)
{
    OutlineMesh mesh{};
    mesh.vertexCount = static_cast<uint32_t>(localTriangles.size());
    if(mesh.vertexCount > 0) {
        static_assert(sizeof(DebugVertex) == sizeof(glm::vec3));
        mesh.buffer = _createMappedBuffer(mesh.vertexCount * sizeof(DebugVertex));
        mesh.buffer->update(localTriangles.data(), mesh.vertexCount * sizeof(DebugVertex));
    }
    m_outlineMeshes[key] = mesh;
}

void DebugRendererVulkan::drawOutline(uint32_t key, const glm::mat4& model, bool selected)
{
    auto it = m_outlineMeshes.find(key);
    if(it == m_outlineMeshes.end() || it->second.vertexCount == 0) {
        return;
    }
    m_outlineDraws.push_back({ key, { model, selected ? 1.0f : 0.5f } });
}

void DebugRendererVulkan::_clearDrawRequests()
{
    m_lineVertices.clear();
    m_outlineDraws.clear();
}

BufferVulkan* DebugRendererVulkan::_createMappedBuffer(size_t bufferSize)
{
    uint32_t bufferID = bufferManagerVulkan->createBuffer2(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    bufferManagerVulkan->mapMemory(bufferID, bufferSize);
    return bufferManagerVulkan->getBuffer(bufferID);
}

void DebugRendererVulkan::_createBuffers()
{
    m_lineVertices.reserve(MAX_DEBUG_VERTICES);

    uint32_t frameCount = VulkanUtils::numFrames();
    for(uint32_t i = 0; i < frameCount; i++) {
        m_vertexBuffers.push_back(_createMappedBuffer(MAX_DEBUG_VERTICES * sizeof(DebugVertex)));
    }
}

void DebugRendererVulkan::_createResources()
{
    auto ubo = rendererManagerVulkan->getUBO();
    width = ubo.width;
    height = ubo.height;

    TextureSamplerConfig samplerConfig {};
    TextureConfig imageConfig {
        .width = width,
        .height = height,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .aspectBits = VK_IMAGE_ASPECT_COLOR_BIT
    };
    imageConfig.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t debugImageID = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
    debugImage = textureManagerVulkan->getTexture(debugImageID);
    textureManagerVulkan->registerBindlessTexture(debugImageID);

    uint32_t displayImageID = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
    displayImage = textureManagerVulkan->getTexture(displayImageID);
    textureManagerVulkan->registerBindlessTexture(displayImageID);

    TextureConfig maskConfig = imageConfig;
    maskConfig.format = VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t maskImageID = textureManagerVulkan->createTexture(maskConfig, samplerConfig);
    maskImage = textureManagerVulkan->getTexture(maskImageID);
    textureManagerVulkan->registerBindlessTexture(maskImageID);
}

void DebugRendererVulkan::_createPipelines()
{
    auto bindingDescription = DebugRendererVulkan::DebugVertex::getBindingDescription();
	auto attributeDescriptions = DebugRendererVulkan::DebugVertex::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	PipelineConfigInfo config = VulkanPipeline::defaultPipelineConfigInfo(1);
    config.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    config.depthStencilInfo.depthTestEnable = VK_FALSE;
    config.depthStencilInfo.depthWriteEnable = VK_FALSE;
	config.renderPass = VK_NULL_HANDLE;

	AttachmentsInfo attachmentsInfo{};
    attachmentsInfo.colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    attachmentsInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    attachmentsInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);

    VkDescriptorSetLayout debugSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
    pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createGraphicsPipelineDynamic(
        "assets/shaders/spv/debug.vert.spv",
        "assets/shaders/spv/debug.frag.spv",
        config,
        attachmentsInfo,
        vertexInputInfo,
        { debugSetLayout, bindlessLayout },
        sizeof(DebugDrawPushConstant)
    );

    // mask like using stencil buffer for highlight selection
    PipelineConfigInfo maskConfig = config;
    maskConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	AttachmentsInfo maskAttachmentsInfo = attachmentsInfo;
    maskAttachmentsInfo.colorAttachmentFormats = { VK_FORMAT_R8G8B8A8_UNORM };

    maskPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    maskPipeline->createGraphicsPipelineDynamic(
        "assets/shaders/spv/debugMask.vert.spv",
        "assets/shaders/spv/debugMask.frag.spv",
        maskConfig,
        maskAttachmentsInfo,
        vertexInputInfo,
        { debugSetLayout },
        sizeof(MaskPushConstant)
    );

    VkDescriptorSetLayout displaySetLayout = descriptorManagerVulkan->getDescriptorLayout(displayLayoutID);
    combinePipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    combinePipeline->createComputePipeline(
        "assets/shaders/spv/debug.comp.spv",
        { displaySetLayout, bindlessLayout },
        sizeof(CombinePushConstant)
    );
}

void DebugRendererVulkan::_createDescriptors()
{
    layoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);

    displayLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	});

	std::vector<VkDescriptorPoolSize> displayPoolSize {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 1},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount * 1},
	};

	displayPoolID = descriptorManagerVulkan->createPool(displayPoolSize, frameCount);
	displaySetsID = descriptorManagerVulkan->createSets(displayLayoutID, displayPoolID, frameCount);
}

void DebugRendererVulkan::_updateDescriptors()
{
    auto uniformBuffersList = rendererManagerVulkan->getBufferUBO();

    auto debugSets = descriptorManagerVulkan->getDescriptorSet(setsID);
	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		DescriptorWriter writer{{}, debugSets[i] };
		descriptorManagerVulkan->writeUniform2(writer, uniformBuffersList[i]->getDescUniformBufferInfo());
		descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
	}

    auto displaySets = descriptorManagerVulkan->getDescriptorSet(displaySetsID);
	for (size_t i = 0; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
		DescriptorWriter writer{{}, displaySets[i] };
		descriptorManagerVulkan->writeUniform2(writer, uniformBuffersList[i]->getDescUniformBufferInfo());
		descriptorManagerVulkan->writeStorageImage2(writer, displayImage->getDescImageInfoGeneral());
		descriptorManagerVulkan->updateDescriptorSets(&writer.writes);
	}
}

void DebugRendererVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	rendererManagerVulkan->setDisplayImage(nullptr);
    _cleanupResources();
    _createResources();
    _updateDescriptors();
    _createPipelines();
}

void DebugRendererVulkan::_cleanupResources()
{
    pipeline->destroy();
    maskPipeline->destroy();
    combinePipeline->destroy();

    textureManagerVulkan->destroy(debugImage->id());
    textureManagerVulkan->destroy(displayImage->id());
    textureManagerVulkan->destroy(maskImage->id());
}

void DebugRendererVulkan::addToGraph(RenderGraphVulkan& graph)
{
}
