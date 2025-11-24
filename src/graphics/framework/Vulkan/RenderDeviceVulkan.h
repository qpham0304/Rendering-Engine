#pragma once

#include "../../src/graphics/RenderDevice.h"
#include "core/VulkanDevice.h"
#include "core/VulkanSwapChain.h"
#include "core/VulkanFrameBuffer.h"
#include "core/VulkanPipeline.h"
#include "core/VulkanBuffer.h"
#include "core/VulkanTexture.h"
#include "core/VulkanShader.h"
#include "core/VulkanCommandPool.h"

class Logger;
class PushConstantData;	// move this to client defined

class RenderDeviceVulkan : public RenderDevice
{
public:	
	//TODO: for quick setup, these should be hidden once done
	VulkanDevice device;
	VulkanSwapChain swapchain;
	VulkanPipeline pipeline;
	VulkanBuffer vulkanBuffer;
	VulkanCommandPool commandPool;
	//VulkanTexture VulkanTexture;
	//VulkanShader VulkanShader;
	//VulkanFrameBuffer VulkanFrameBuffer;
	VkBuffer stagingBuffer;
	VkImageView textureImageView;
	VkSampler textureSampler;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	VkImage textureImage;
	VkDeviceMemory textureImageMemory;

public:
	RenderDeviceVulkan();
	~RenderDeviceVulkan();

					
	virtual int init(WindowConfig platform) override;
	virtual int onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void render() override;
	virtual void endFrame() override;
	virtual void draw(uint32_t numIndicies = 0, uint32_t numInstances = 1) override;
	virtual const uint32_t& getCurrentFrame() const;
	virtual const uint32_t& getImageIndex() const;

	virtual void* getNativeInstance() override;
	virtual void* getNativeDevice() override;
	virtual void* getPhysicalDevice() override;
	virtual void* getNativeRenderPass(int id) override { return nullptr; };
	virtual CommandBufferHandle* getCommandBuffer(int id) override { return nullptr; };
	virtual DeviceInfo getDeviceInfo() const override;
	virtual PipelineInfo getPipelineInfo() const override;

	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers);
	void createTextureImage();
	void createTextureImageView();
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void createTextureSampler();
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void createDepthResources();
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();
	bool hasStencilComponent(VkFormat format);

	void setPushConstantRange(uint32_t range);
	void setViewport();
	void setScissor();
	void shaderSetUniform();	//TODO: move to shader, let shader control descriptor set binding
	void waitIdle();

protected:
	Logger& Log() const;

private:
	Logger* m_logger{ nullptr };
	uint32_t pushConstantRange = 0;

	uint32_t currentFrame = 0;
	uint32_t imageIndex = 0;



	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;


private:

	void _cleanup();
};

