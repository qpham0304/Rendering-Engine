#include "ImageBasedRendererVulkan.h"

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
	: Renderer("ImageBasedRendererVulkan")
{

}

ImageBasedRendererVulkan::~ImageBasedRendererVulkan()
{

}

bool ImageBasedRendererVulkan::init(WindowConfig config)
{
	Service::init(config);

	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerSPD");
	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	bufferManagerVulkan = &static_cast<BufferManagerVulkan&>(bufferManager);
	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = &static_cast<DescriptorManagerVulkan&>(descriptorManager);
	
	textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
	meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
	modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
	guiManager = &ServiceLocator::GetService<GuiManager>("ImGuiManager");

	hdrImageID = textureManager->loadTexture(
		// "assets/textures/hdr/photo_studio_loft_hall_2k.hdr", 
		"assets/textures/hdr/industrial_sunset_02_puresky_1k.hdr", 
		1, 
		false
	);

	hdrImage = dynamic_cast<TextureVulkan*>(textureManager->getTexture(hdrImageID));
	assert(hdrImage && "Failed to cast texture to TextureVulkan");

	groupCountX = (hdrImage->width() + 15) / 16;
	groupCountY = (hdrImage->height() + 15) / 16;
	totalWorkgroups = groupCountX * groupCountY;
	size_t bufferSize = totalWorkgroups * 9 * sizeof(PartialSum);
	bufferManagerVulkan->createStorageBuffers(partialSumBuffers, bufferSize);

	_createDescriptorSetProjection();
	VkDescriptorSetLayout descriptorLayout = descriptorManagerVulkan->getDescriptorLayout(projectionSH_descriptorLayoutID);
	projectionSH_pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	projectionSH_pipeline->createComputePipeline(
		"assets/shaders/projectionSH.comp.spv", 
		{ descriptorLayout },
		0
	);

	bufferManagerVulkan->createStorageBuffers(finalSumBuffers, sizeof(FinalSH));

	_createDescriptorSetGlobalSum();
	VkDescriptorSetLayout descriptorLayoutSum = descriptorManagerVulkan->getDescriptorLayout(sumSH_descriptorLayoutID);
	sumSH_pipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	sumSH_pipeline->createComputePipeline(
		"assets/shaders/globalSumSH.comp.spv", 
		{ descriptorLayoutSum },
		sizeof(uint32_t)
	);
	

    return true;
}

bool ImageBasedRendererVulkan::onClose()
{
	projectionSH_pipeline->destroy();
	sumSH_pipeline->destroy();

    return true;
}

void ImageBasedRendererVulkan::onUpdate()
{
}

void ImageBasedRendererVulkan::render(Camera &camera)
{
	// VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	// computeSH(cmdBuffer, renderDeviceVulkan->getCurrentFrameIndex());
	// debugSHResults(renderDeviceVulkan->getCurrentFrameIndex());
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
}