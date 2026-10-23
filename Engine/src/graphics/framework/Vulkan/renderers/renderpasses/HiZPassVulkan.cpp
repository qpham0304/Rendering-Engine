#include "HiZPassVulkan.h"
#include <core/features/camera.h>
#include <core/scene/SceneManager.h>
#include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>

HiZPassVulkan::HiZPassVulkan(std::string serviceName)
	:	PostProcessRendererVulkan(serviceName)
{

}

HiZPassVulkan::~HiZPassVulkan()
{

}

bool HiZPassVulkan::init(WindowConfig config)
{
    // hack to preven double initilization by the deferred renderer and dependency cycle
	// a depth prepass before gbuffer pass which will resolve the issue
    if (m_initialized) {
        return true;
    }
    m_initialized = true;

    PostProcessRendererVulkan::init(config);
        
    RendererVulkan* renderer = nullptr;
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = geometryPass->renderTarget.depthTexture;
    normalImage = geometryPass->renderTarget.gBufferNorm;

    
    _createResources();
    _createDescriptors();
    _createPipelines();

    return true;
}

bool HiZPassVulkan::onClose()
{
    PostProcessRendererVulkan::onClose();

	for (auto view : HiZMipViews) {
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(renderDeviceVulkan->device, view, nullptr);
		}
	}

    HiZMipViews.clear();
    
    pipeline->destroy();
    
    return true;
}

void HiZPassVulkan::onUpdate()
{
    PostProcessRendererVulkan::onUpdate();

}

void HiZPassVulkan::render(Camera &camera)
{
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "HiZ Pass");
    writeHiZ(cmd, currentFrame);
	renderDeviceVulkan->endLabel(cmd);
}

void HiZPassVulkan::writeHiZ(VkCommandBuffer cmd, uint32_t currentFrame)
{
    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
    glm::vec2 gBufferDimensions = { (float)geometryPass->renderTarget.width, (float)geometryPass->renderTarget.height };

    for (uint32_t i = 0; i < mipLevels; i++) {
        uint32_t currentWidth  = std::max(1u, mapSize >> i);
        uint32_t currentHeight = std::max(1u, mapSize >> i);
        glm::vec2 prevDims;
        if (i == 0) {
            prevDims = glm::vec2(geometryPass->renderTarget.width, geometryPass->renderTarget.height);
        } else {
            prevDims = glm::vec2(std::max(1u, mapSize >> (i - 1)));
        }
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipelineLayout, 0, 1, &HiZMipSets[i], 0, nullptr);
        pushConstant.u_PreviousLevelDimensions = prevDims;
        pushConstant.u_MipLevel = i;

        vkCmdPushConstants(
            cmd, 
            pipeline->pipelineLayout, 
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, 
            sizeof(PushConstant), 
            &pushConstant
        );
        
        vkCmdDispatch(cmd, (currentWidth + 15) / 16, (currentHeight + 15) / 16, 1);

        VkImageMemoryBarrier mipBarrier{};
        mipBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        mipBarrier.image = hizImage->textureImage;
        mipBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        mipBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // Stay in General, but flush caches
        mipBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        mipBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        // CRITICAL: Only transition the mip level we just finished writing
        mipBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            0, 0, nullptr, 0, nullptr, 1, &mipBarrier);
    }

}

void HiZPassVulkan::_createResources()
{
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(mapSize))) + 1;

	// fixed size, owned by the render graph: created once, never touched by a window resize
	RGTextureDesc desc{};
	desc.config.width = mapSize;
	desc.config.height = mapSize;
	desc.config.format = VK_FORMAT_R32_SFLOAT;
	desc.config.mipLevels = mipLevels;
	desc.config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	desc.sampler.filter = VK_FILTER_NEAREST;
	desc.sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

	RenderGraphVulkan& graph = rendererManagerVulkan->getRenderGraph();
	uint32_t previousImageID = hizImage ? hizImage->id() : 0;
	hizHandle = graph.createRenderTexture("HiZ.Depth", desc);
	hizImage = graph.getTexture(hizHandle);

	// compared by id, a recreated texture can land at the same address and the old mip views would point at freed memory
	if(hizImage->id() == previousImageID) {
		return;
	}

	// a new texture, rebuild the per mip views every level of the chain is written through
	for (auto view : HiZMipViews) {
		vkDestroyImageView(renderDeviceVulkan->device, view, nullptr);
	}
	HiZMipViews.resize(mipLevels);
	for (uint32_t j = 0; j < mipLevels; j++) {
		TextureManagerVulkan::createImageView(
			hizImage->textureImage,
			HiZMipViews[j],
			VK_FORMAT_R32_SFLOAT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			1,
			j,
			1,
			VK_IMAGE_VIEW_TYPE_2D,
			renderDeviceVulkan->device
		);
	}
}

void HiZPassVulkan::_createPipelines()
{
    VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(hiZLayoutID);
    
	pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
    pipeline->createComputePipeline(
		"assets/shaders/spv/HiZ.comp.spv", 
		{ layout }, 
		sizeof(PushConstant)
	);
}

void HiZPassVulkan::_createDescriptors()
{
	hiZLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
	});

	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mipLevels },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, mipLevels }
	};
	hiZPoolID = descriptorManagerVulkan->createPool(poolSizes, mipLevels);

	// one set per mip, the image is a single copy so there are no per frame sets
	hiZSetsID = descriptorManagerVulkan->createSets(hiZLayoutID, hiZPoolID, mipLevels);
	HiZMipSets = descriptorManagerVulkan->getDescriptorSet(hiZSetsID);

	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	TextureVulkan* depth = geometryPass->renderTarget.depthTexture;

	for (uint32_t i = 0; i < mipLevels; i++) {
		VkDescriptorImageInfo sourceInfo{};
		sourceInfo.sampler = hizImage->textureSampler;

		if (i == 0) {   // Mip 0 reads from the actual G-Buffer Depth
			sourceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			sourceInfo.imageView = depth->textureImageView;
		} else {        // Mips 1+ read from the PREVIOUS Hi-Z mip, which stays in GENERAL
			sourceInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			sourceInfo.imageView = HiZMipViews[i - 1];
		}

		std::vector<VkWriteDescriptorSet> writes;
		VkDescriptorImageInfo outputInfo{};
		outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		outputInfo.imageView = HiZMipViews[i];

		descriptorManagerVulkan->writeImage(&writes, HiZMipSets[i], 0, sourceInfo);
		descriptorManagerVulkan->writeStorageImage(&writes, HiZMipSets[i], 1, outputInfo);

		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}

}

void HiZPassVulkan::_updateDescriptor(uint32_t index)
{

}

void HiZPassVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
    _cleanupResources();
    _createResources();
	_createDescriptors();
	_createPipelines();
}

void HiZPassVulkan::_cleanupResources()
{
    pipeline->destroy();
}

void HiZPassVulkan::addToGraph(RenderGraphVulkan& graph)
{
	auto geometryPass = dynamic_cast<GeometryPassVulkan*>(rendererManagerVulkan->getRenderer("GeometryPassVulkan"));
	depthImage = geometryPass->renderTarget.depthTexture;
	normalImage = geometryPass->renderTarget.gBufferNorm;
	_createResources();		// fixed size, the graph hands back the same texture every frame

	if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

	auto depth = graph.importTexture(depthImage);

	RenderPassNode node{};
	node.name = "HiZ";
	node.reads  = { { depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };	// mip 0 samples the g-buffer depth
	node.writes = { { hizHandle, VK_IMAGE_LAYOUT_GENERAL } };				// every mip is a storage image

	node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
		Camera* camera = SceneManager::cameraController;
		if(!camera) {
			return;
		}

		render(*camera);
	};

	graph.addPass(node);
}
