#pragma once

#include "graphics/RenderDevice.h"
#include "core/VulkanDevice.h"
#include "core/VulkanSwapChain.h"
#include "core/VulkanPipeline.h"
#include "core/VulkanCommandPool.h"
#include "core/VulkanUtils.h"
#include "resources/descriptors/VulkanDescriptorManager.h"
#include "resources/buffers/VulkanBufferManager.h"

class Logger;
class PushConstantData;	// move this to client defined
class TextureVulkan;

class RenderDeviceVulkan : public RenderDevice
{
public:	
	//TODO: for quick setup, some of these should be hidden or moved outside once done
	VulkanDevice device;
	VulkanSwapChain swapchain;
	VulkanPipeline pipeline;
	VulkanBufferManager vulkanBufferManager;
	VulkanCommandPool commandPool;
	
	TextureVulkan* texture{ nullptr };

	TextureVulkan* depthTexture{ nullptr };
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;


	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	VkDescriptorSetLayout imguiDescriptorSetLayout;
	VkDescriptorPool imguiDescriptorPool;
	VkDescriptorSet imguiTextureDescriptorSet; 


public:
	RenderDeviceVulkan();
	virtual ~RenderDeviceVulkan() override;

					
	virtual int init(WindowConfig platform) override;
	virtual int onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void render() override;
	virtual void endFrame() override;
	virtual void draw(uint32_t numIndicies = 0, uint32_t numInstances = 1) override;
	virtual const uint32_t& getCurrentFrameIndex() const;
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
	void createDescriptorSets();

	//void createTextureImage();
	//void createTextureImageView();
	//void createTextureSampler();
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	
	void createDepthResources();
	void createImage(
		uint32_t width, 
		uint32_t height, 
		VkFormat format, 
		VkImageTiling tiling, 
		VkImageUsageFlags usage, 
		VkMemoryPropertyFlags properties, 
		VkImage& image, 
		VkDeviceMemory& imageMemory
	);
	

	void createTextureViewDescriptorSet();

	VkFormat findSupportedFormat(
		const std::vector<VkFormat>& candidates, 
		VkImageTiling tiling, 
		VkFormatFeatureFlags features
	);
	VkFormat findDepthFormat();
	bool hasStencilComponent(VkFormat format);

	void setPushConstantRange(uint32_t range);
	void setViewport();
	void setScissor();
	void waitIdle();
	Logger& Log() const;

private:
	Logger* m_logger{ nullptr };
	uint32_t pushConstantRange = 0;
	uint32_t currentFrame = 0;
	std::atomic<uint16_t> m_ids;
	uint16_t activeCommandPool = 0;
	
	private:
	uint16_t _assignID();
	void _cleanup();
};

