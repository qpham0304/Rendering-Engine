#include "DLSSPassVulkan.h"
#include "GeometryPassVulkan.h"
#include "DeferredCombinePassVulkan.h"
#include <core/scene/SceneManager.h>
#include <core/features/camera.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/vulkan/core/DLSSSupport.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <filesystem>

#ifdef WK_DLSS
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>
#endif

namespace {
	float halton(uint32_t index, uint32_t base)
	{
		float f = 1.0f;
		float result = 0.0f;
		while(index > 0) {
			f /= static_cast<float>(base);
			result += f * static_cast<float>(index % base);
			index /= base;
		}
		return result;
	}

#ifdef WK_DLSS
	NVSDK_NGX_PerfQuality_Value perfQuality(int mode)
	{
		switch(mode) {
			case 0: return NVSDK_NGX_PerfQuality_Value_DLAA;
			case 2: return NVSDK_NGX_PerfQuality_Value_Balanced;
			case 3: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
			case 4: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
			default: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
		}
	}

	NVSDK_NGX_Resource_VK imageResource(TextureVulkan* texture, VkImageAspectFlags aspect, bool readWrite)
	{
		VkImageSubresourceRange range { aspect, 0, 1, 0, 1 };
		return NVSDK_NGX_Create_ImageView_Resource_VK(texture->textureImageView, texture->textureImage, range,
			texture->format, texture->width(), texture->height(), readWrite);
	}
#endif
}

DLSSPassVulkan::DLSSPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{
    setFootprint("DLSSPassVulkan", sizeof(DLSSPassVulkan), MemorySystem::Graphics);
}

DLSSPassVulkan::~DLSSPassVulkan()
{
}

bool DLSSPassVulkan::init(WindowConfig config)
{
    PostProcessRendererVulkan::init(config);

    settings = &rendererManagerVulkan->getGraphicsSettings().add<DLSSSettings>("DLSS");
    _initNGX();
    settings->supported = m_ngxReady;

    _fetchInputImages();
    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool DLSSPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();

    _releaseFeature();
    _shutdownNGX();
    pipeline->destroy();

    rendererManagerVulkan->getGraphicsSettings().remove<DLSSSettings>();
    settings = nullptr;

    return true;
}

void DLSSPassVulkan::onUpdate()
{
    PostProcessRendererVulkan::onUpdate();
}

void DLSSPassVulkan::_initNGX()
{
#ifdef WK_DLSS
    VulkanDevice& device = renderDeviceVulkan->device;
    if(!device.dlssSupport()) {
        m_logger->warn("DLSS is not available, the device was created without the extensions NGX needs");
        return;
    }

    std::filesystem::path dataPath = std::filesystem::temp_directory_path() / "MyGraphicsEngine";
    std::filesystem::create_directories(dataPath);

    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init_with_ProjectID(
        DLSSSupport::ProjectID, NVSDK_NGX_ENGINE_TYPE_CUSTOM, DLSSSupport::EngineVersion, dataPath.wstring().c_str(),
        device.getInstance(), device.getPhysicalDevice(), device, vkGetInstanceProcAddr, vkGetDeviceProcAddr, DLSSSupport::featureInfo()
    );
    if(NVSDK_NGX_FAILED(result)) {
        m_logger->warn("NGX init failed ({:#x}), DLSS is disabled", static_cast<uint32_t>(result));
        return;
    }

    if(NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_params))) {
        m_logger->warn("NGX capability parameters unavailable, DLSS is disabled");
        NVSDK_NGX_VULKAN_Shutdown1(device);
        return;
    }

    int available = 0;
    NVSDK_NGX_Parameter_GetI(m_params, NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    if(!available) {
        int needsDriver = 0;
        NVSDK_NGX_Parameter_GetI(m_params, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsDriver);
        m_logger->warn(needsDriver ? "DLSS needs a newer NVIDIA driver, DLSS is disabled" : "DLSS is not supported on this gpu");
        _shutdownNGX();
        return;
    }

    m_ngxReady = true;
    m_logger->info("DLSS super resolution ready");
#endif
}

void DLSSPassVulkan::_shutdownNGX()
{
#ifdef WK_DLSS
    if(m_params) {
        NVSDK_NGX_VULKAN_DestroyParameters(m_params);
        m_params = nullptr;
        NVSDK_NGX_VULKAN_Shutdown1(renderDeviceVulkan->device);
    }
    m_ngxReady = false;
#endif
}

bool DLSSPassVulkan::isActive() const
{
    return m_ngxReady && settings && settings->enabled;
}

// asks ngx for the input size of the selected mode, cached since it only changes with the output or the mode
VkExtent2D DLSSPassVulkan::getRenderExtent(VkExtent2D outputExtent)
{
    int mode = std::clamp(settings->mode, 0, 4);
    if(mode == m_queriedMode && outputExtent.width == m_queriedOutput.width && outputExtent.height == m_queriedOutput.height) {
        return m_queriedRender;
    }

    VkExtent2D renderExtent = outputExtent;
#ifdef WK_DLSS
    uint32_t optimalWidth = 0, optimalHeight = 0, maxWidth = 0, maxHeight = 0, minWidth = 0, minHeight = 0;
    float sharpness = 0.0f;
    NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(m_params, outputExtent.width, outputExtent.height, perfQuality(mode),
        &optimalWidth, &optimalHeight, &maxWidth, &maxHeight, &minWidth, &minHeight, &sharpness);
    if(NVSDK_NGX_SUCCEED(result) && optimalWidth > 0 && optimalHeight > 0) {
        renderExtent = { optimalWidth, optimalHeight };
    }
#endif

    m_queriedMode = mode;
    m_queriedOutput = outputExtent;
    m_queriedRender = renderExtent;
    settings->renderWidth = renderExtent.width;
    settings->renderHeight = renderExtent.height;
    return renderExtent;
}

// halton 2,3 with enough phases to cover every output pixel inside a render pixel
glm::vec2 DLSSPassVulkan::getJitter(uint64_t frameNumber) const
{
    float ratio = m_queriedRender.width > 0 ? static_cast<float>(m_queriedOutput.width) / static_cast<float>(m_queriedRender.width) : 1.0f;
    uint32_t phases = std::max(8u, static_cast<uint32_t>(std::ceil(8.0f * ratio * ratio)));
    uint32_t index = static_cast<uint32_t>(frameNumber % phases) + 1;
    return { halton(index, 2) - 0.5f, halton(index, 3) - 0.5f };
}

bool DLSSPassVulkan::_createFeature(VkCommandBuffer cmd, VkExtent2D renderExtent, VkExtent2D outputExtent)
{
#ifdef WK_DLSS
    int mode = std::clamp(settings->mode, 0, 4);

    NVSDK_NGX_DLSS_Create_Params createParams{};
    createParams.Feature.InWidth = renderExtent.width;
    createParams.Feature.InHeight = renderExtent.height;
    createParams.Feature.InTargetWidth = outputExtent.width;
    createParams.Feature.InTargetHeight = outputExtent.height;
    createParams.Feature.InPerfQualityValue = perfQuality(mode);
    createParams.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR
        | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes
        | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

    NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT1(renderDeviceVulkan->device, cmd, 1, 1, &m_feature, m_params, &createParams);
    if(NVSDK_NGX_FAILED(result)) {
        m_logger->error("DLSS feature creation failed ({:#x}), DLSS is disabled", static_cast<uint32_t>(result));
        m_feature = nullptr;
        settings->enabled = false;
        return false;
    }

    m_featureRender = renderExtent;
    m_featureOutput = outputExtent;
    m_featureMode = mode;
    m_logger->info("DLSS mode {} upscaling {}x{} to {}x{}", mode, renderExtent.width, renderExtent.height, outputExtent.width, outputExtent.height);
    return true;
#else
    return false;
#endif
}

void DLSSPassVulkan::_releaseFeature()
{
#ifdef WK_DLSS
    if(m_feature) {
        renderDeviceVulkan->waitIdle();
        NVSDK_NGX_VULKAN_ReleaseFeature(m_feature);
        m_feature = nullptr;
    }
#endif
}

void DLSSPassVulkan::_fetchInputImages()
{
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	auto combinePass = dynamic_cast<DeferredCombinePassVulkan*>(rendererManagerVulkan->getRenderer("DeferredCombinePassVulkan"));

    colorImage = combinePass->getOutputImage();
    depthImage = geometryPass->renderTarget.depthTexture;
    motionImage = geometryPass->renderTarget.gBufferMotion;
}

void DLSSPassVulkan::_createResources()
{
    RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();

    RGTextureDesc motionDesc{};
    motionDesc.config.format = VK_FORMAT_R16G16_SFLOAT;
    motionDesc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dlssMotionHandle = graph.createRenderTexture("DLSS.Motion", motionDesc);
    dlssMotionImage = graph.getTexture(dlssMotionHandle);

    RGTextureDesc outputDesc{};
    outputDesc.config.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    outputDesc.config.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;    // ngx clears the output itself
    outputDesc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    outputDesc.outputSized = true;
    outputHandle = graph.createRenderTexture("DLSS.Output", outputDesc);
    outputImage = graph.getTexture(outputHandle);
    outputImageID = outputImage->id();
}

void DLSSPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(layoutID);

	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline(
		"assets/shaders/spv/dlssMotion.comp.spv",
		{ layout },
		sizeof(MotionPushConstant)
	);
}

void DLSSPassVulkan::_createDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings {
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    };

	uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 2 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, frameCount },
	};

    layoutID = descriptorManagerVulkan->createLayout(bindings);
    poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);
	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, frameCount);

    for(uint32_t i = 0; i < frameCount; i++) {
        _updateDescriptor(i);
    }
}

void DLSSPassVulkan::_updateDescriptor(uint32_t index)
{
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

    VkDescriptorImageInfo depthInfo{};
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthInfo.imageView = depthImage->textureImageView;
	depthInfo.sampler = depthImage->textureSampler;

    VkDescriptorImageInfo motionInfo{};
	motionInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	motionInfo.imageView = motionImage->textureImageView;
	motionInfo.sampler = motionImage->textureSampler;

    VkDescriptorImageInfo outputInfo{};
	outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputInfo.imageView = dlssMotionImage->textureImageView;
	outputInfo.sampler = dlssMotionImage->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 0, depthInfo);
	descriptorManagerVulkan->writeImage(&writes, descriptorSets[index], 1, motionInfo);
	descriptorManagerVulkan->writeStorageImage(&writes, descriptorSets[index], 2, outputInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);
}

// the gbuffer clears the sky's motion to a placeholder, dlss needs the camera rotation there or the sky smears
void DLSSPassVulkan::writeMotion(VkCommandBuffer cmd, uint32_t currentFrame)
{
    _updateDescriptor(currentFrame);

    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    auto descriptorSet = descriptorManagerVulkan->getDescriptorSet(setsID)[currentFrame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MotionPushConstant), &motionPush);

    uint32_t groupX = (dlssMotionImage->width() + 15) / 16;
    uint32_t groupY = (dlssMotionImage->height() + 15) / 16;
    vkCmdDispatch(cmd, groupX, groupY, 1);
}

void DLSSPassVulkan::evaluate(VkCommandBuffer cmd)
{
#ifdef WK_DLSS
    VkExtent2D renderExtent { colorImage->width(), colorImage->height() };
    VkExtent2D outputExtent { outputImage->width(), outputImage->height() };
    bool featureStale = !m_feature
        || renderExtent.width != m_featureRender.width || renderExtent.height != m_featureRender.height
        || outputExtent.width != m_featureOutput.width || outputExtent.height != m_featureOutput.height
        || std::clamp(settings->mode, 0, 4) != m_featureMode;
    if(featureStale) {
        _releaseFeature();
        if(!_createFeature(cmd, renderExtent, outputExtent)) {
            return;
        }
        m_resetHistory = true;
    }

    NVSDK_NGX_Resource_VK color = imageResource(colorImage, VK_IMAGE_ASPECT_COLOR_BIT, false);
    NVSDK_NGX_Resource_VK depth = imageResource(depthImage, VK_IMAGE_ASPECT_DEPTH_BIT, false);
    NVSDK_NGX_Resource_VK motion = imageResource(dlssMotionImage, VK_IMAGE_ASPECT_COLOR_BIT, false);
    NVSDK_NGX_Resource_VK output = imageResource(outputImage, VK_IMAGE_ASPECT_COLOR_BIT, true);

    glm::vec2 jitter = rendererManagerVulkan->getJitterPixels();

    NVSDK_NGX_VK_DLSS_Eval_Params evalParams{};
    evalParams.Feature.pInColor = &color;
    evalParams.Feature.pInOutput = &output;
    evalParams.pInDepth = &depth;
    evalParams.pInMotionVectors = &motion;
    evalParams.InJitterOffsetX = jitter.x;
    evalParams.InJitterOffsetY = jitter.y;
    evalParams.InRenderSubrectDimensions = { renderExtent.width, renderExtent.height };
    evalParams.InReset = m_resetHistory ? 1 : 0;
    // the gbuffer stores current minus previous in uv, dlss wants previous minus current in render pixels
    evalParams.InMVScaleX = -static_cast<float>(renderExtent.width);
    evalParams.InMVScaleY = -static_cast<float>(renderExtent.height);

    NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(cmd, m_feature, m_params, &evalParams);
    if(NVSDK_NGX_FAILED(result)) {
        m_logger->error("DLSS evaluate failed ({:#x}), DLSS is disabled", static_cast<uint32_t>(result));
        settings->enabled = false;
    }
    m_resetHistory = false;
#endif
}

void DLSSPassVulkan::render(Camera& camera)
{
	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "DLSS");
    evaluate(cmd);
	renderDeviceVulkan->endLabel(cmd);
}

void DLSSPassVulkan::_recreateResources()
{
    m_resetHistory = true;
}

void DLSSPassVulkan::_cleanupResources()
{
}

void DLSSPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
    if(!isActive()) {
        m_resetHistory = true;
        return;
    }

    if(needResize) {
        _recreateResources();
        needResize = false;
    }
    if(settings->resetHistory) {
        m_resetHistory = true;
        settings->resetHistory = false;
    }

    _fetchInputImages();
    _createResources();     // same textures unless the render or output size changed

    Camera* camera = SceneManager::cameraController;
    if(camera) {
        glm::mat4 proj = camera->getProjectionMatrix();
        proj[1][1] *= -1.0f;
        glm::mat4 skyViewProj = proj * glm::mat4(glm::mat3(camera->getViewMatrix()));
        if(m_resetHistory) {
            m_prevSkyViewProj = skyViewProj;
        }
        motionPush.skyReproject = m_prevSkyViewProj * glm::inverse(skyViewProj);
        m_prevSkyViewProj = skyViewProj;
    }
    motionPush.jitter = rendererManagerVulkan->getUBO().jitter;

    auto color = graph.importTexture(colorImage);
    auto depth = graph.importTexture(depthImage);
    auto motion = graph.importTexture(motionImage);

    RenderPassNode motionNode{};
    motionNode.settings = settings;
    motionNode.name = "DLSS.Motion";
    motionNode.reads = {
        { depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { motion, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
    };
    motionNode.writes = { { dlssMotionHandle, VK_IMAGE_LAYOUT_GENERAL } };
    motionNode.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
        writeMotion(cmd, currentFrame);
    };
    graph.addPass(motionNode);

    RenderPassNode node{};
    node.settings = settings;
    node.name = "DLSS";
    node.reads = {
        { color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { dlssMotionHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
    };
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
