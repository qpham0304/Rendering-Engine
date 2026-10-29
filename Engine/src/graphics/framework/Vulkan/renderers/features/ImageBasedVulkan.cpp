#include "ImageBasedVulkan.h"

#include "core/features/ServiceLocator.h"
#include "graphics/renderers/RenderDevice.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <core/scene/SceneManager.h>
#include <graphics/framework/Vulkan/renderers/RenderGraphVulkan.h>
#include <volk.h>

ImageBasedRendererVulkan::ImageBasedRendererVulkan()
	: RendererVulkan("ImageBasedRendererVulkan")
{
	setFootprint("ImageBasedRendererVulkan", sizeof(ImageBasedRendererVulkan), MemorySystem::Graphics);

}

ImageBasedRendererVulkan::~ImageBasedRendererVulkan()
{

}

bool ImageBasedRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	settings = &rendererManagerVulkan->getGraphicsSettings().add<ImageBasedSettings>("Environment");
	loadedEnvironmentMap = settings->environmentMap;

	hdrImageID = textureManagerVulkan->loadTexture(
		loadedEnvironmentMap,
		1,
		false
	);

	// the missing checker is shared, the transition below would discard it, so use a fallback sky color
	if(hdrImageID == 0) {
		m_logger->error("Environment map {} failed to load, using a flat sky color", loadedEnvironmentMap);
		const float skyColor[4] = { 0.5f, 0.6f, 0.75f, 1.0f };
		hdrImageID = textureManagerVulkan->createTextureFromPixels(skyColor, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, "$fallback$environment", false);
	}

	hdrImage = textureManagerVulkan->getTexture(hdrImageID);
	if(!hdrImage) {
		m_logger->critical("ImageBasedRendererVulkan: no environment texture");
		return false;
	}

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
    hdrImage->transitImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
    renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

	fileEnvironment = hdrImage;

	groupCountX = (hdrImage->width() + 15) / 16;
	groupCountY = (hdrImage->height() + 15) / 16;
	totalWorkgroups = groupCountX * groupCountY;

	// room for environments up to 4k x 2k, a different map or the sky can be swapped in without reallocating
	maxWorkgroups = std::max(totalWorkgroups, ((4096u + 15u) / 16u) * ((2048u + 15u) / 16u));
	size_t bufferSize = maxWorkgroups * 9 * sizeof(PartialSum);
	bufferManagerVulkan->createStorageBuffers(partialSumBuffers, bufferSize);
	bufferManagerVulkan->createStorageBuffers(finalSumBuffers, sizeof(FinalSH));

	_createDescriptorSetProjection();
	_createDescriptorSetGlobalSum();
	_createResourceLUT();
	_createResourcePrefilteredMap();

	textureManagerVulkan->registerBindlessTexture(brdfLUT->id());
	textureManagerVulkan->registerBindlessTexture(prefilterMap->id());
	textureManagerVulkan->registerBindlessTexture(hdrImage->id());
	
	writeBRDF();

    return true;
}

bool ImageBasedRendererVulkan::onClose()
{
	rendererManagerVulkan->getGraphicsSettings().remove<ImageBasedSettings>();
	settings = nullptr;

	for (auto view : prefilterMipViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(renderDeviceVulkan->device, view, nullptr);
        }
    }
    prefilterMipViews.clear();
	
	projectionSH_pipeline->destroy();
	sumSH_pipeline->destroy();
	brdfLUT_pipeline->destroy();
	prefilter_pipeline->destroy();
	
    return true;
}

void ImageBasedRendererVulkan::onUpdate()
{

}

void ImageBasedRendererVulkan::render(Camera &camera)
{
	_loadEnvironmentIfChanged();

	// a new environment was loaded. the descriptors are updated here instead of in loadTexture
	// because the frame in flight may still read the old ones
	if(hdrImage_temp) {
		renderDeviceVulkan->waitIdle();
		_updateEnvironmentDescriptors();
		hdrImage_temp = nullptr;
		environmentReady = false;
	}

	if(environmentReady) {
		return;
	}

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
	renderDeviceVulkan->beginLabel(cmd, "IBL Pass");
	for (uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
		computeSH(cmd, i);
	}
	computePrefilter(cmd, 0);	// writes every mip itself
	renderDeviceVulkan->endLabel(cmd);

	environmentReady = true;
}

void ImageBasedRendererVulkan::computeSH(VkCommandBuffer cmd, uint32_t currentFrame) {
	projectionSH_pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(projectionSH_descriptorSetID);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		projectionSH_pipeline->pipelineLayout,
		0,
		1,
		&descriptorSets[currentFrame],
		0,
		nullptr
	);

    vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

    // wait for Pass 1 to finish
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.buffer = static_cast<VkBuffer>(*partialSumBuffers[currentFrame]);
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
		cmd, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        0, 
		0, 
		nullptr, 
		1, 
		&barrier, 
		0, 
		nullptr
	);

	sumSH_pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

	auto descriptorSetsSum = descriptorManagerVulkan->getDescriptorSet(sumSH_descriptorSetID);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		sumSH_pipeline->pipelineLayout,
		0,
		1,
		&descriptorSetsSum[currentFrame],
		0,
		nullptr
	);

	uint32_t numPartials = groupCountX * groupCountY;
    vkCmdPushConstants(
		cmd, 
		sumSH_pipeline->pipelineLayout, 
		VK_SHADER_STAGE_COMPUTE_BIT, 
		0, 
		sizeof(uint32_t), 
		&numPartials
	);

	vkCmdDispatch(cmd, 1, 1, 1);

    // wait for Pass 2 to finish
	VkBufferMemoryBarrier barrierSum = {};
    barrierSum.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrierSum.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrierSum.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrierSum.buffer = static_cast<VkBuffer>(*finalSumBuffers[currentFrame]);
    barrierSum.offset = 0;
    barrierSum.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
		cmd, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        0, 
		0, 
		nullptr, 
		1, 
		&barrierSum, 
		0, 
		nullptr
	);

}

void ImageBasedRendererVulkan::writeBRDF() {
	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
	brdfLUT->transitImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, 1);

	brdfLUT_pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
	
	auto lutDescriptorSet = descriptorManagerVulkan->getDescriptorSet(lutDescriptorSetID)[0];
	
	vkCmdBindDescriptorSets(
		cmd, 
		VK_PIPELINE_BIND_POINT_COMPUTE, 
		brdfLUT_pipeline->pipelineLayout, 
		0, 
		1, 
		&lutDescriptorSet, 
		0, 
		nullptr
	);
	
	vkCmdDispatch(cmd, 512 / 16, 512 / 16, 1);

	brdfLUT->transitImage(cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
	renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);
}

void ImageBasedRendererVulkan::computePrefilter(VkCommandBuffer cmd, uint32_t currentFrame) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;	// stay in GENERAL while write to all the mips.
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.image = prefilterMap->textureImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );

    prefilter_pipeline->bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

    for (uint32_t i = 0; i < mipLevels; i++) {
        uint32_t mipSize = mapSize >> i;
        
        PrefilterPushConstants push{};
        push.roughness = (float)i / (float)(mipLevels - 1);
        push.mipSize = mipSize;
        vkCmdPushConstants(
			cmd,
			prefilter_pipeline->pipelineLayout,
			VK_SHADER_STAGE_COMPUTE_BIT,
			0,
			sizeof(push),
			&push
		);

        vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			prefilter_pipeline->pipelineLayout,
			0,
			1,
			&prefilterSets[i],
			0,
			nullptr
		);

        uint32_t groups = std::max(1u, (mipSize + 15) / 16);
        vkCmdDispatch(cmd, groups, groups, 6); 
    }

	VkImageMemoryBarrier finalBarrier = {};
	finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	finalBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL; 					// where compute left
	finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 	// where graphics reads
	finalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	finalBarrier.image = prefilterMap->textureImage;
	finalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	finalBarrier.subresourceRange.levelCount = mipLevels;
	finalBarrier.subresourceRange.layerCount = 6;

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &finalBarrier
	);
}

void ImageBasedRendererVulkan::loadTexture(std::string_view path) {
	hdrImageID_temp = textureManagerVulkan->loadTexture(
		path, 
		1, 
		false
	);

	if(hdrImageID_temp == 0) {
		m_logger->error("Environment map {} failed to load, keeping the current one", path);
		hdrImage_temp = nullptr;
		return;
	}

	hdrImage_temp = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(hdrImageID_temp));

	if(hdrImage_temp) {
		fileEnvironment = hdrImage_temp;

		// while the sky provides the environment, the new map waits until the sky is turned off
		if(environmentOverride) {
			hdrImage_temp = nullptr;
			return;
		}

		int32_t temp = hdrImageID;
		hdrImage = hdrImage_temp;
		hdrImageID = hdrImageID_temp;

		// textureManagerVulkan->destroy(temp);
	}
	// sh and prefilter are recomputed by render() at the start of the next frame
}

 // another pass provides the environment for sky atmosphere, nullptr fall back to environment map
void ImageBasedRendererVulkan::setEnvironmentOverride(TextureVulkan* texture)
{
	environmentOverride = texture;
	TextureVulkan* target = texture ? texture : fileEnvironment;
	if(!target || target->id() == hdrImageID) {
		return;
	}

	// same path as loading a new map: render() swaps the descriptors once the frames in flight are done with them
	hdrImage = target;
	hdrImageID = target->id();
	hdrImage_temp = target;
}

void ImageBasedRendererVulkan::_updateEnvironmentDescriptors()
{
	// the sh projection runs one workgroup per 16x16 texels of whatever environment is bound now
	groupCountX = (hdrImage->width() + 15) / 16;
	groupCountY = (hdrImage->height() + 15) / 16;
	totalWorkgroups = groupCountX * groupCountY;
	if(totalWorkgroups > maxWorkgroups) {
		m_logger->error("environment {}x{} is larger than the sh buffers hold, sh only covers part of it", hdrImage->width(), hdrImage->height());
		groupCountY = maxWorkgroups / groupCountX;
		totalWorkgroups = groupCountX * groupCountY;
	}

	VkDescriptorImageInfo imageInfo{ hdrImage->textureSampler, hdrImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

	auto projectionSets = descriptorManagerVulkan->getDescriptorSet(projectionSH_descriptorSetID);
	for (uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
		std::vector<VkWriteDescriptorSet> writes;
		descriptorManagerVulkan->writeImage(&writes, projectionSets[i], 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}

	for (uint32_t i = 0; i < mipLevels; i++) {
		std::vector<VkWriteDescriptorSet> writes;
		descriptorManagerVulkan->writeImage(&writes, prefilterSets[i], 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}


void ImageBasedRendererVulkan::_createDescriptorSetProjection() {
	projectionSH_descriptorLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount }
	};

	projectionSH_descriptorPoolID = descriptorManagerVulkan->createPool(
		poolSizes, 
		frameCount
	);

	projectionSH_descriptorSetID = descriptorManagerVulkan->createSets(
		projectionSH_descriptorLayoutID, 
		projectionSH_descriptorPoolID, 
		frameCount
	);

	auto projectionSH_descriptorSets = descriptorManagerVulkan->getDescriptorSet(projectionSH_descriptorSetID);

	for(int i = 0; i < frameCount; i++) {
		VkDescriptorImageInfo imageInfo{}; 
		imageInfo.sampler = hdrImage->textureSampler;
		imageInfo.imageView = hdrImage->textureImageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorBufferInfo storageBufferInfo{};
		storageBufferInfo.buffer = static_cast<VkBuffer>(*partialSumBuffers[i]);
		storageBufferInfo.offset = 0;
		storageBufferInfo.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet imageWrite{};
		imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageWrite.dstSet = projectionSH_descriptorSets[i];
		imageWrite.dstBinding = 0;
		imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imageWrite.descriptorCount = 1;
		imageWrite.pImageInfo = &imageInfo;

		VkWriteDescriptorSet bufferWrite{};
		bufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		bufferWrite.dstSet = projectionSH_descriptorSets[i];
		bufferWrite.dstBinding = 1;
		bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bufferWrite.descriptorCount = 1;
		bufferWrite.pBufferInfo = &storageBufferInfo;

		std::array<VkWriteDescriptorSet, 2> writes = { imageWrite, bufferWrite };

		vkUpdateDescriptorSets(renderDeviceVulkan->device, 2, writes.data(), 0, nullptr);
	}

	VkDescriptorSetLayout descriptorLayout = descriptorManagerVulkan->getDescriptorLayout(projectionSH_descriptorLayoutID);
	projectionSH_pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	projectionSH_pipeline->createComputePipeline(
		"assets/shaders/spv/projectionSH.comp.spv", 
		{ descriptorLayout },
		0
	);
}

void ImageBasedRendererVulkan::_createDescriptorSetGlobalSum() {
	sumSH_descriptorLayoutID = descriptorManagerVulkan->createLayout({
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 2 }
	};

	sumSH_descriptorPoolID = descriptorManagerVulkan->createPool(
		poolSizes, 
		frameCount
	);

	sumSH_descriptorSetID = descriptorManagerVulkan->createSets(
		sumSH_descriptorLayoutID, 
		sumSH_descriptorPoolID, 
		frameCount
	);

	auto sumSH_descriptorSet = descriptorManagerVulkan->getDescriptorSet(sumSH_descriptorSetID);

	for(int i = 0; i < frameCount; i++) {
		VkDescriptorBufferInfo storageBufferInfo1{};
		storageBufferInfo1.buffer = static_cast<VkBuffer>(*partialSumBuffers[i]);
		storageBufferInfo1.offset = 0;
		storageBufferInfo1.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet bufferWrite1{};
		bufferWrite1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		bufferWrite1.dstSet = sumSH_descriptorSet[i];
		bufferWrite1.dstBinding = 1;
		bufferWrite1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bufferWrite1.descriptorCount = 1;
		bufferWrite1.pBufferInfo = &storageBufferInfo1;
		
		VkDescriptorBufferInfo storageBufferInfo2{};
		storageBufferInfo2.buffer = static_cast<VkBuffer>(*finalSumBuffers[i]);
		storageBufferInfo2.offset = 0;
		storageBufferInfo2.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet bufferWrite2{};
		bufferWrite2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		bufferWrite2.dstSet = sumSH_descriptorSet[i];
		bufferWrite2.dstBinding = 2;
		bufferWrite2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bufferWrite2.descriptorCount = 1;
		bufferWrite2.pBufferInfo = &storageBufferInfo2;

		std::array<VkWriteDescriptorSet, 2> writes = { bufferWrite1, bufferWrite2 };

		vkUpdateDescriptorSets(renderDeviceVulkan->device, 2, writes.data(), 0, nullptr);
	}

	VkDescriptorSetLayout descriptorLayoutSum = descriptorManagerVulkan->getDescriptorLayout(sumSH_descriptorLayoutID);
	sumSH_pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	sumSH_pipeline->createComputePipeline(
		"assets/shaders/spv/globalSumSH.comp.spv", 
		{ descriptorLayoutSum },
		sizeof(uint32_t)
	);
}

void ImageBasedRendererVulkan::_createResourceLUT() {
	TextureConfig imageConfig {
		.width = 512,
		.height = 512,
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	uint32_t textureID = textureManagerVulkan->createTexture(imageConfig, TextureSamplerConfig{});
	brdfLUT = textureManagerVulkan->getTexture(textureID);

	lutDescriptorLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};
	
	lutDescriptorPoolID = descriptorManagerVulkan->createPool(poolSizes, 1);

	lutDescriptorSetID = descriptorManagerVulkan->createSets(lutDescriptorLayoutID, lutDescriptorPoolID, 1);
	auto lutDescriptorSets = descriptorManagerVulkan->getDescriptorSet(lutDescriptorSetID);

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = brdfLUT->textureImageView;
	imageInfo.sampler = brdfLUT->textureSampler;

	std::vector<VkWriteDescriptorSet> writes;
	descriptorManagerVulkan->writeStorageImage(&writes, lutDescriptorSets[0], 0, imageInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);

	
	VkDescriptorSetLayout descriptorLayoutLUT = descriptorManagerVulkan->getDescriptorLayout(lutDescriptorLayoutID);
	brdfLUT_pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	brdfLUT_pipeline->createComputePipeline(
		"assets/shaders/spv/brdfLUT.comp.spv", 
		{ descriptorLayoutLUT }, 
		0
	);
}

void ImageBasedRendererVulkan::_createResourcePrefilteredMap() {
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(mapSize))) + 1;

	TextureConfig imageConfig {
		.width = mapSize,
		.height = mapSize,
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.mipLevels = mipLevels,
		.layerCount = 6,
		.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	uint32_t textureID = textureManagerVulkan->createTexture(imageConfig, TextureSamplerConfig{});
	prefilterMap = textureManagerVulkan->getTexture(textureID);

		
	prefilterMipViews.resize(mipLevels);
	for (uint32_t i = 0; i < mipLevels; i++) {
		TextureManagerVulkan::createImageView(
			prefilterMap->textureImage, 
			prefilterMipViews[i], 
			VK_FORMAT_R16G16B16A16_SFLOAT, 
			VK_IMAGE_ASPECT_COLOR_BIT, 
			1,                          	// only want to see one level
			i,
			6,                          	// 6 faces
			VK_IMAGE_VIEW_TYPE_2D_ARRAY, 	// compute sees cube as an array
			renderDeviceVulkan->device
		);
	}

	prefilterLayoutID = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr } 
	});

	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mipLevels * frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, mipLevels * frameCount }
	};
	
	prefilterPoolID = descriptorManagerVulkan->createPool(poolSizes, mipLevels * frameCount);

	uint32_t prefilterSetsID = descriptorManagerVulkan->createSets(prefilterLayoutID, prefilterPoolID, mipLevels);
	prefilterSets = descriptorManagerVulkan->getDescriptorSet(prefilterSetsID);

	for (uint32_t i = 0; i < mipLevels; i++) {
		std::vector<VkWriteDescriptorSet> writes;
		
		VkDescriptorImageInfo sourceInfo{};
		sourceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		sourceInfo.imageView = hdrImage->textureImageView;
		sourceInfo.sampler = hdrImage->textureSampler;

		VkDescriptorImageInfo outputInfo{};
		outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		outputInfo.imageView = prefilterMipViews[i];

		descriptorManagerVulkan->writeImage(&writes, prefilterSets[i], 0, sourceInfo);
		descriptorManagerVulkan->writeStorageImage(&writes, prefilterSets[i], 1, outputInfo);
		
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
	prefilterMap->transitImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 6);
	renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

	
	VkDescriptorSetLayout descriptorLayoutPrefilter = descriptorManagerVulkan->getDescriptorLayout(prefilterLayoutID);
	prefilter_pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	prefilter_pipeline->createComputePipeline(
		"assets/shaders/spv/prefilter.comp.spv", 
		{ descriptorLayoutPrefilter }, 
		sizeof(PrefilterPushConstants)
	);
}


void ImageBasedRendererVulkan::_recreateResources()
{
	m_logger->warn("recource recreation unimlemented");
}

void ImageBasedRendererVulkan::_cleanupResources()
{
	m_logger->warn("recource cleanup unimlemented");
}

void ImageBasedRendererVulkan::_loadEnvironmentIfChanged()
{
	if(settings->environmentMap == loadedEnvironmentMap || settings->environmentMap.empty()) {
		return;
	}
	loadedEnvironmentMap = settings->environmentMap;
	loadTexture(loadedEnvironmentMap);
}

void ImageBasedRendererVulkan::addToGraph(RenderGraphVulkan& graph)
{
	_loadEnvironmentIfChanged();

	auto hdr = graph.importTexture(hdrImage);
	auto prefilter = graph.importTexture(prefilterMap);

	RenderPassNode node{};
	node.name = "IBL";
	node.reads  = { { hdr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };
	node.writes = { { prefilter, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };

	node.execute = [this](VkCommandBuffer cmd, uint32_t currentFrame) {
		Camera* camera = SceneManager::cameraController;
		if(!camera) {
			return;
		}
		render(*camera);
	};

	graph.addPass(node);
}
