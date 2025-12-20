#pragma once

#include "graphics/renderers/RenderDevice.h"
#include "graphics/framework/vulkan/core/VulkanDevice.h"
#include "graphics/framework/vulkan/core/VulkanSwapChain.h"
#include "graphics/framework/vulkan/core/VulkanPipeline.h"
#include "graphics/framework/vulkan/core/VulkanCommandPool.h"
#include "graphics/framework/vulkan/core/VulkanUtils.h"

class Logger;

class RenderDeviceVulkan : public RenderDevice
{
public:	
	//TODO: for quick setup, some of these should be hidden or moved outside once done
	VulkanDevice device;
	VulkanSwapChain swapchain;
	VulkanPipeline pipeline;
	VulkanCommandPool commandPool;

public:
	RenderDeviceVulkan();
	virtual ~RenderDeviceVulkan() override;

	virtual bool init(WindowConfig platform) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void render() override;
	virtual void endFrame() override;
	virtual void draw(uint32_t numIndicies = 0, uint32_t numInstances = 1, uint32_t offset = 0) override;
	virtual const uint32_t& getCurrentFrameIndex() const;
	virtual const uint32_t& getImageIndex() const;

	virtual void* getNativeInstance() override;
	virtual void* getNativeDevice() override;
	virtual void* getPhysicalDevice() override;
	virtual void* getNativeRenderPass(int id) override { return nullptr; };
	virtual CommandBufferHandle* getCommandBuffer(int id) override { return nullptr; };
	virtual DeviceInfo getDeviceInfo() const override;
	virtual PipelineInfo getPipelineInfo() const override;

	bool hasStencilComponent(VkFormat format);

	void setViewport();
	void setScissor();
	void waitIdle();
	Logger& Log() const;

private:
	Logger* m_logger{ nullptr };
	uint32_t currentFrame = 0;
	std::atomic<uint16_t> m_ids;
	uint16_t activeCommandPool = 0;
	
private:
	uint16_t _assignID();
	void _cleanup();
};

