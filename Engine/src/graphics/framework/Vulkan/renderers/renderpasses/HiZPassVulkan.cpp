#include "HiZPassVulkan.h"
#include <core/features/camera.h>
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
    PostProcessRendererVulkan::init(config);
        
    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];

    
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
    if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

    RendererVulkan* renderer = nullptr;
    renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
	
    uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
    depthImage = deferredRendererVulkan->renderTarget.depthTextures[currentFrame];
    normalImage = deferredRendererVulkan->renderTarget.gBufferNorm[currentFrame];

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    writeHiZ(cmd, currentFrame);
}

void HiZPassVulkan::writeHiZ(VkCommandBuffer cmd, uint32_t currentFrame)
{
    TextureManagerVulkan::transitionImageLayout(
        cmd, hiZImage->textureImage, VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, mipLevels, 1, renderDeviceVulkan);

    pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    RendererVulkan* renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
    auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
    glm::vec2 gBufferDimensions = { (float)deferredRendererVulkan->renderTarget.width, (float)deferredRendererVulkan->renderTarget.height };

    for (uint32_t i = 0; i < mipLevels; i++) {
        uint32_t currentWidth  = std::max(1u, mapSize >> i);
        uint32_t currentHeight = std::max(1u, mapSize >> i);
        glm::vec2 prevDims;
        if (i == 0) {
            prevDims = glm::vec2(deferredRendererVulkan->renderTarget.width, 
                                deferredRendererVulkan->renderTarget.height);
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
        mipBarrier.image = hiZImage->textureImage;
        mipBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        mipBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // Stay in General, but flush caches
        mipBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        mipBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        // CRITICAL: Only transition the mip level we just finished writing!
        mipBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            0, 0, nullptr, 0, nullptr, 1, &mipBarrier);
    }

    TextureManagerVulkan::transitionImageLayout(
        cmd, hiZImage->textureImage, VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        mipLevels, 1, renderDeviceVulkan
    );
}

void HiZPassVulkan::_createResources()
{
    uint32_t textureID = textureManagerVulkan->createTexture();
	hiZImage = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(textureID));

	assert(hiZImage && "failed to cast texture into vulkan texture");

	mipLevels = static_cast<uint32_t>(std::floor(std::log2(mapSize))) + 1;

	TextureManagerVulkan::createImage(
		mapSize, 
		mapSize, 
		VK_FORMAT_R32_SFLOAT, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		hiZImage->textureImage, 
		hiZImage->textureImageMemory,
		mipLevels, 
		1,
		0,
		renderDeviceVulkan->device
	);

	
	TextureManagerVulkan::createImageView(
		hiZImage->textureImage, 
		hiZImage->textureImageView, 
		VK_FORMAT_R32_SFLOAT, 
		VK_IMAGE_ASPECT_COLOR_BIT, 
		mipLevels,
		0,
		1,
		VK_IMAGE_VIEW_TYPE_2D,
		renderDeviceVulkan->device
	);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(mipLevels);

	TextureManagerVulkan::createTextureSampler(
		hiZImage->textureSampler,
		renderDeviceVulkan->device,
		samplerInfo
	);

		
	HiZMipViews.resize(mipLevels);
	for (uint32_t i = 0; i < mipLevels; i++) {
		TextureManagerVulkan::createImageView(
			hiZImage->textureImage, 
			HiZMipViews[i], 
			VK_FORMAT_R32_SFLOAT, 
			VK_IMAGE_ASPECT_COLOR_BIT, 
			1,
			i,
			1,                          	
			VK_IMAGE_VIEW_TYPE_2D,
			renderDeviceVulkan->device
		);
	}

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
	TextureManagerVulkan::transitionImageLayout(
		cmd,
		hiZImage->textureImage, 
		VK_FORMAT_R32_SFLOAT,
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		mipLevels, 
		1, 
		renderDeviceVulkan
	);
	renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);
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

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mipLevels * frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, mipLevels * frameCount }
	};
	hiZPoolID = descriptorManagerVulkan->createPool(poolSizes, mipLevels * frameCount);

	hiZSetsID = descriptorManagerVulkan->createSets(hiZLayoutID, hiZPoolID, mipLevels);
	HiZMipSets = descriptorManagerVulkan->getDescriptorSet(hiZSetsID);

	for (uint32_t i = 0; i < mipLevels; i++) {
        VkDescriptorImageInfo sourceInfo{};
        sourceInfo.sampler = hiZImage->textureSampler;

        if (i == 0) {   // Mip 0 reads from the actual G-Buffer Depth
            sourceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            sourceInfo.imageView = depthImage->textureImageView;
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
    textureManagerVulkan->destroy(hiZImage->id());
    pipeline->destroy();
}
