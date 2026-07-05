#include "ImageBasedVulkan.h"

#include "core/features/ServiceLocator.h"
#include "graphics/renderers/RenderDevice.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <gui/GuiManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <core/scene/SceneManager.h>
#include <imgui.h>
#include <vulkan/vulkan.h>

ImageBasedRendererVulkan::ImageBasedRendererVulkan()
	: RendererVulkan("ImageBasedRendererVulkan")
{

}

ImageBasedRendererVulkan::~ImageBasedRendererVulkan()
{

}

bool ImageBasedRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	hdrImageID = textureManagerVulkan->loadTexture(
		// "assets/textures/hdr/farm_field_puresky_2k.hdr", 
		"assets/textures/hdr/newport_loft.hdr", 
		// "assets/textures/hdr/meadow_1k.hdr", 
		// "assets/textures/night-skybox/top.jpg", 
		// "assets/textures/hdr/photo_studio_loft_hall_2k.hdr", 
		1, 
		false
	);

	hdrImage = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(hdrImageID));
	assert(hdrImage && "Failed to cast texture to TextureVulkan");

	VkCommandBuffer cmd = renderDeviceVulkan->commandPool.beginSingleTimeCommand();
    hdrImage->transitImage(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
    renderDeviceVulkan->commandPool.endSingleTimeCommand(cmd);

	groupCountX = (hdrImage->width() + 15) / 16;
	groupCountY = (hdrImage->height() + 15) / 16;
	totalWorkgroups = groupCountX * groupCountY;
	
	size_t bufferSize = totalWorkgroups * 9 * sizeof(PartialSum);
	bufferManagerVulkan->createStorageBuffers(partialSumBuffers, bufferSize);
	bufferManagerVulkan->createStorageBuffers(finalSumBuffers, sizeof(FinalSH));

	_createDescriptorSetProjection();
	_createDescriptorSetGlobalSum();
	_createResourceLUT();
	_createResourcePrefilteredMap();
	
	writeBRDF();

    return true;
}

bool ImageBasedRendererVulkan::onClose()
{
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
	if(hdrImage_temp) {
		uint32_t frameCount = VulkanUtils::numFrames();
		renderDeviceVulkan->waitIdle();
		VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
		
		//update spherical harmonic descriptors
		auto projectionSets = descriptorManagerVulkan->getDescriptorSet(projectionSH_descriptorSetID);
		for (uint32_t i = 0; i < frameCount; i++) {
			VkDescriptorImageInfo imageInfo{ hdrImage->textureSampler, hdrImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			std::vector<VkWriteDescriptorSet> writes;
			descriptorManagerVulkan->writeImage(&writes, projectionSets[i], 0, imageInfo);
			descriptorManagerVulkan->updateDescriptorSets(&writes);
			computeSH(cmdBuffer, i);
		}

		//update prefilter descriptors
		for (uint32_t i = 0; i < mipLevels; i++) {
			VkDescriptorImageInfo imageInfo{ hdrImage->textureSampler, hdrImage->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			std::vector<VkWriteDescriptorSet> writes;
			descriptorManagerVulkan->writeImage(&writes, prefilterSets[i], 0, imageInfo);
			descriptorManagerVulkan->updateDescriptorSets(&writes);
			computePrefilter(cmdBuffer, i);
		}
		hdrImage_temp = nullptr;
	}
	
}

void ImageBasedRendererVulkan::render(Camera &camera)
{
	
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

	hdrImage_temp = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(hdrImageID_temp));

	if(hdrImage_temp) {
		int32_t temp = hdrImageID;
		hdrImage = hdrImage_temp;
		hdrImageID = hdrImageID_temp;

		// textureManagerVulkan->destroy(temp);
	}
	
	onUpdate();
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
	uint32_t textureID = textureManagerVulkan->createTexture();
	brdfLUT = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(textureID));

	assert(brdfLUT && "failed to cast texture into vulkan texture");

	TextureManagerVulkan::createImage(
		512, 
		512, 
		VK_FORMAT_R16G16B16A16_SFLOAT, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		brdfLUT->textureImage, 
		brdfLUT->textureImageMemory, 
		1, 
		renderDeviceVulkan->device
	);

	TextureManagerVulkan::createImageView(
		brdfLUT->textureImage, 
		brdfLUT->textureImageView, 
		VK_FORMAT_R16G16B16A16_SFLOAT, 
		VK_IMAGE_ASPECT_COLOR_BIT, 
		1, 
		renderDeviceVulkan->device
	);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	TextureManagerVulkan::createTextureSampler(
		brdfLUT->textureSampler,
		renderDeviceVulkan->device,
		samplerInfo
	);

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
	uint32_t textureID = textureManagerVulkan->createTexture();
	prefilterMap = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(textureID));

	assert(prefilterMap && "failed to cast texture into vulkan texture");

	mipLevels = static_cast<uint32_t>(std::floor(std::log2(mapSize))) + 1;

    // auto createTexture = [&] (TextureVulkan*& texture, uint32_t w, uint32_t h, VkFormat format) {
    //     TextureSamplerConfig samplerConfig = { VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR };
    //     TextureConfig imageConfig { .width = w, .height = h, .format = format};

    //     uint32_t id = textureManagerVulkan->createTexture(imageConfig, samplerConfig);
    //     texture = dynamic_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));
    // };

	// createTexture(prefilterMap, mapSize, mapSize, VK_FORMAT_R16G16B16A16_SFLOAT);

	TextureManagerVulkan::createImage(
		mapSize, 
		mapSize, 
		VK_FORMAT_R16G16B16A16_SFLOAT, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		prefilterMap->textureImage, 
		prefilterMap->textureImageMemory,
		mipLevels, 
		6,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		renderDeviceVulkan->device
	);

	
	TextureManagerVulkan::createImageView(
		prefilterMap->textureImage, 
		prefilterMap->textureImageView, 
		VK_FORMAT_R16G16B16A16_SFLOAT, 
		VK_IMAGE_ASPECT_COLOR_BIT, 
		mipLevels,
		0,
		6,
		VK_IMAGE_VIEW_TYPE_CUBE,
		renderDeviceVulkan->device
	);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(mipLevels);

	TextureManagerVulkan::createTextureSampler(
		prefilterMap->textureSampler,
		renderDeviceVulkan->device,
		samplerInfo
	);

		
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