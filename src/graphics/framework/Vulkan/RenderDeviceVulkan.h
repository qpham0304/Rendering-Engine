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
#include "core/VulkanCommandPool.h"

class Logger;

class RenderDeviceVulkan : public RenderDevice
{
public:	
	//TODO: for quick setup, these should be hidden once done
	VulkanDevice device;
	VulkanSwapChain swapchain;
	VulkanPipeline pipeline;
	VulkanBuffer vulkanBuffer;
	VulkanCommandPool vulkanCommandPool;
	//VulkanTexture VulkanTexture;
	//VulkanShader VulkanShader;
	//VulkanFrameBuffer VulkanFrameBuffer;

	uint32_t currentFrame = 0;
	uint32_t imageIndex = 0;


	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	struct PushConstantData {
		alignas(16) glm::vec3 color;
		alignas(16) glm::vec3 range;
		alignas(4)  bool flag;
		alignas(4)  float data;
	};


public:
	RenderDeviceVulkan();
	~RenderDeviceVulkan();

	virtual int init(WindowConfig platform) override;
	virtual int onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void render() override;
	virtual void endFrame() override;

	virtual void* getNativeInstance() override { return nullptr; };
	virtual void* getNativeDevice() override { return nullptr; };
	virtual void* getPhysicalDevice() override { return nullptr; };
	virtual CommandBufferHandle* getCommandBuffer(int id) override { return nullptr; };
	virtual void* getNativeRenderPass(int id) override { return nullptr; };
	virtual DeviceInfo getDeviceInfo(int id) override { return DeviceInfo(); };
	virtual PipelineInfo getPipelineInfo(int id) override { return PipelineInfo(); };

	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers);

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

protected:
	Logger& Log() const;

private:
	Logger* m_logger;

	void _cleanup();
};

