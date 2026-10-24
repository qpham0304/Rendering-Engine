#include "DebugRendererVulkan.h"

#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/renderpasses/GeometryPassVulkan.h>
#include "core/features/ServiceLocator.h"
#include "core/features/Camera.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"

DebugRendererVulkan::DebugRendererVulkan(std::string serviceName)
    : RendererVulkan(serviceName)
{
    setFootprint("DebugRendererVulkan", sizeof(DebugRendererVulkan), MemorySystem::Graphics);

}

DebugRendererVulkan::~DebugRendererVulkan()
{

}

bool DebugRendererVulkan::init(WindowConfig config)
{
    RendererVulkan::init(config);

    geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

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

    
    // the grid needs the scene depth, which only the deferred path leaves at this size
    TextureVulkan* depthImage = geometryPass ? geometryPass->renderTarget.depthTexture : nullptr;
    bool gridEnabled = gridVisible && rendererManagerVulkan->getRenderMode() == 1 && depthImage
        && geometryPass->renderTarget.width == width && geometryPass->renderTarget.height == height;
    if(gridEnabled) {
        _buildGrid(camera.getPosition());
    } else {
        m_gridVertices.clear();
    }

    // if no draw data is empty/no one request draw debug, exit and keep the scene image untouched
    TextureVulkan* sceneImage = rendererManagerVulkan->getDisplayImage();
    bool hasOutlines = !m_outlineDraws.empty();
    bool hasGrid = !m_gridVertices.empty();
    if(!sceneImage || sceneImage == displayImage || (m_lineVertices.empty() && !hasOutlines && !hasGrid)) {
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

        // the grid goes in first and clears the debug image, the lines then draw over it without a depth test
        if(hasGrid) {
            _renderGrid(cmd, currentFrame, depthImage);
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }

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

// lines are split into segments so the alpha can fade out radially instead of ending in a hard square
void DebugRendererVulkan::_buildGrid(const glm::vec3& cameraPosition)
{
    const int halfLines = 50;
    const int segmentsPerLine = 10;
    const float spacing = 1.0f;
    const float extent = halfLines * spacing;
    const float segmentLength = 2.0f * extent / segmentsPerLine;

    // snapped under the camera so the grid seems endless while its lines stay on whole units
    glm::vec3 center(std::round(cameraPosition.x / spacing) * spacing, 0.0f, std::round(cameraPosition.z / spacing) * spacing);

    auto fadeAt = [&](const glm::vec3& p) {
        float distance = glm::length(glm::vec2(p.x - center.x, p.z - center.z)) / extent;
        return 1.0f - glm::smoothstep(0.35f, 1.0f, distance);
    };

    // world axes in red (x) and blue (z), every tenth line brighter
    auto lineColor = [&](float worldCoordinate, const glm::vec4& axisColor) {
        int index = static_cast<int>(std::round(worldCoordinate / spacing));
        if(index == 0) {
            return axisColor;
        }
        return index % 10 == 0 ? glm::vec4(0.65f, 0.65f, 0.65f, 0.45f) : glm::vec4(0.55f, 0.55f, 0.55f, 0.22f);
    };

    auto addLine = [&](const glm::vec3& start, const glm::vec3& direction, const glm::vec4& color) {
        for(int s = 0; s < segmentsPerLine; s++) {
            glm::vec3 a = start + direction * (segmentLength * s);
            glm::vec3 b = a + direction * segmentLength;
            m_gridVertices.push_back({ a, glm::vec4(glm::vec3(color), color.a * fadeAt(a)) });
            m_gridVertices.push_back({ b, glm::vec4(glm::vec3(color), color.a * fadeAt(b)) });
        }
    };

    m_gridVertices.clear();
    for(int i = -halfLines; i <= halfLines; i++) {
        float x = center.x + i * spacing;
        float z = center.z + i * spacing;
        addLine(glm::vec3(x, 0.0f, center.z - extent), glm::vec3(0.0f, 0.0f, 1.0f), lineColor(x, glm::vec4(0.27f, 0.45f, 0.95f, 0.9f)));
        addLine(glm::vec3(center.x - extent, 0.0f, z), glm::vec3(1.0f, 0.0f, 0.0f), lineColor(z, glm::vec4(0.95f, 0.3f, 0.3f, 0.9f)));
    }
}

void DebugRendererVulkan::_renderGrid(VkCommandBuffer cmd, uint32_t currentFrame, TextureVulkan* depthImage)
{
    uint32_t vertexCount = std::min<uint32_t>(m_gridVertices.size(), MAX_GRID_VERTICES);
    m_gridBuffers[currentFrame]->update(m_gridVertices.data(), vertexCount * sizeof(GridVertex));

    // the graph leaves the gbuffer depth readable, it is only tested against here, never written
    depthImage->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = debugImage->textureImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = { {0.0f, 0.0f, 0.0f, 0.0f} };

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthImage->textureImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {width, height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    auto debugSets = descriptorManagerVulkan->getDescriptorSet(setsID);
    VkBuffer vertexBuffer = static_cast<VkBuffer>(*m_gridBuffers[currentFrame]);
    VkDeviceSize offset = 0;

    vkCmdBeginRendering(cmd, &renderingInfo);
    renderDeviceVulkan->setViewport(width, height);
    renderDeviceVulkan->setScissor(width, height);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline->pipelineLayout, 0, 1, &debugSets[currentFrame], 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
    vkCmdEndRendering(cmd);

    depthImage->transitImage(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
        m_gridBuffers.push_back(_createMappedBuffer(MAX_GRID_VERTICES * sizeof(GridVertex)));
    }
    m_gridVertices.reserve(MAX_GRID_VERTICES);
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

    // grid: colored lines tested against the scene depth, color stays unpremultiplied for the combine pass
    auto gridBinding = GridVertex::getBindingDescription();
    auto gridAttributes = GridVertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo gridVertexInput = vertexInputInfo;
    gridVertexInput.pVertexBindingDescriptions = &gridBinding;
    gridVertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(gridAttributes.size());
    gridVertexInput.pVertexAttributeDescriptions = gridAttributes.data();

    PipelineConfigInfo gridConfig = config;
    gridConfig.depthStencilInfo.depthTestEnable = VK_TRUE;
    gridConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
    gridConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    for(auto& attachment : gridConfig.colorBlendAttachments) {
        attachment.blendEnable = VK_TRUE;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    AttachmentsInfo gridAttachmentsInfo = attachmentsInfo;
    gridAttachmentsInfo.depthAttachmentFormat = depthFormat;

    gridPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    gridPipeline->createGraphicsPipelineDynamic(
        "assets/shaders/spv/debugGrid.vert.spv",
        "assets/shaders/spv/debugGrid.frag.spv",
        gridConfig,
        gridAttachmentsInfo,
        gridVertexInput,
        { debugSetLayout },
        sizeof(DebugDrawPushConstant)
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
    gridPipeline->destroy();
    maskPipeline->destroy();
    combinePipeline->destroy();

    textureManagerVulkan->destroy(debugImage->id());
    textureManagerVulkan->destroy(displayImage->id());
    textureManagerVulkan->destroy(maskImage->id());
}

void DebugRendererVulkan::addToGraph(RenderGraphVulkan& graph)
{
}
