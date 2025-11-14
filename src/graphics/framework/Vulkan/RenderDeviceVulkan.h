#pragma once

#include "../../src/graphics/RenderDevice.h"

#include <memory>

#include "core/VulkanDevice.h"
#include "core/VulkanSwapChain.h"
#include "core/VulkanFrameBuffer.h"
#include "core/VulkanPipeline.h"
#include "core/VulkanBuffer.h"
#include "core/VulkanTexture.h"
#include "core/VulkanShader.h"

class Logger;

class RenderDeviceVulkan : public RenderDevice
{
public:	
	//TODO: for quick setup, these should be hidden once done
	VulkanDevice device;
	VulkanSwapChain swapchain;
	VulkanFrameBuffer VulkanFrameBuffer;
	VulkanPipeline VulkanPipeline;
	//VulkanBuffer VulkanBuffer;
	//VulkanTexture VulkanTexture;
	//VulkanShader VulkanShader;

public:
	RenderDeviceVulkan();
	~RenderDeviceVulkan() override = default;

	virtual int init(WindowConfig platform) override;
	virtual int onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;

	virtual void* getNativeInstance() override { return nullptr; };
	virtual void* getNativeDevice() override { return nullptr; };
	virtual void* getPhysicalDevice() override { return nullptr; };
	virtual CommandBufferHandle* getCommandBuffer(int id) override { return nullptr; };
	virtual void* getNativeRenderPass(int id) override { return nullptr; };
	virtual DeviceInfo getDeviceInfo(int id) override { return DeviceInfo(); };
	virtual PipelineInfo getPipelineInfo(int id) override { return PipelineInfo(); };

protected:
	Logger& Log() const;

private:
	Logger* m_logger;

};

