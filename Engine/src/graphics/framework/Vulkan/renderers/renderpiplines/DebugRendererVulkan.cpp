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

    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool DebugRendererVulkan::onClose()
{
    return true;
}

void DebugRendererVulkan::onUpdate()
{
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    
    pushConstant.imageIndex = rendererManagerVulkan->getRenderTextures("forwardColorOut")[currentFrame]->id();

    combinePush.sceneIndex = rendererManagerVulkan->getRenderTextures("forwardColorOut")[currentFrame]->id();
    combinePush.debugSceneIndex = debugImage->id();

    m_vertexBuffer->update(m_lineVertices.data(), MAX_DEBUG_VERTICES * sizeof(DebugVertex));
}

void DebugRendererVulkan::render(Camera &camera)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = debugImage->textureImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = { {0.1f, 0.1f, 0.1f, 1.0f} };

    std::vector<VkRenderingAttachmentInfo> colorAttachments = { colorAttachment };

    auto ubo = rendererManagerVulkan->getUBO();
    uint32_t width = ubo.width;
    uint32_t height = ubo.height;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, {width, height} };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = nullptr;
    // renderingInfo.pDepthAttachment = &depthAttachment;

    auto bindlessSet = descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0];
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    renderDeviceVulkan->beginLabel(cmd, "debug render");
    {
        auto debugSets = descriptorManagerVulkan->getDescriptorSet(setsID);
        debugImage->transitImage(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        vkCmdBeginRendering(cmd, &renderingInfo);
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, &debugSets[currentFrame], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 1, 1, &bindlessSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DebugDrawPushConstant), &pushConstant);

        m_vertexBuffer->bind(cmd);
        vkCmdDraw(cmd, static_cast<uint32_t>(m_lineVertices.size()), 1, 0, 0);
        
        renderDeviceVulkan->setViewport();
        renderDeviceVulkan->setScissor();

        vkCmdEndRendering(cmd);
        debugImage->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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

        vkCmdDispatch(cmd, (ubo.width + 7) / 8, (ubo.height + 7) / 8, 1);
        displayImage->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    renderDeviceVulkan->endLabel(cmd);

    
	rendererManagerVulkan->setDisplayImage(displayImage);
}

void DebugRendererVulkan::drawLine(const glm::vec3 &start, const glm::vec3 &end)
 {
    m_lineVertices.push_back({start});
    m_lineVertices.push_back({end});
}

void DebugRendererVulkan::_createResources()
{
    m_lineVertices.reserve(MAX_DEBUG_VERTICES);
    int bufferSize = MAX_DEBUG_VERTICES * sizeof(DebugVertex);
    // uint32_t bufferID = bufferManagerVulkan->createVertexBuffer(m_lineVertices.data(), bufferSize);

    uint32_t bufferID = bufferManagerVulkan->createBuffer2(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    m_vertexBuffer = bufferManagerVulkan->getBuffer(bufferID);
    
    bufferManagerVulkan->mapMemory(bufferID, bufferSize);
    m_vertexBuffer->update(m_lineVertices.data(), bufferSize);

    auto ubo = rendererManagerVulkan->getUBO();
    TextureSamplerConfig samplerConfig {};
    TextureConfig imageConfig { 
        .width = static_cast<uint32_t>(ubo.width),
        .height = static_cast<uint32_t>(ubo.height),
        .format = renderDeviceVulkan->swapchain.swapChainImageFormat, 
        .aspectBits = VK_IMAGE_ASPECT_COLOR_BIT
    };
    
    uint32_t debugImageID = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
    debugImage = textureManagerVulkan->getTexture(debugImageID);

    imageConfig.format = VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t displayImageID = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
    displayImage = textureManagerVulkan->getTexture(displayImageID);
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
	config.renderPass = VK_NULL_HANDLE;
    // config.setAdditiveBlend();

	AttachmentsInfo attachmentsInfo{};
    // attachmentsInfo.colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    attachmentsInfo.colorAttachmentFormats = { renderDeviceVulkan->swapchain.swapChainImageFormat };
    attachmentsInfo.depthAttachmentFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
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

    _updateDescriptors();
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

}

void DebugRendererVulkan::_cleanupResources()
{
    pipeline->destroy();
    combinePipeline->destroy();
}
