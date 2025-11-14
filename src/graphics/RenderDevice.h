#pragma once

#include "../../../src/services/Service.h"

class RenderDevice : public Service
{
public:
	struct CommandBufferHandle {
		void* commandBuffer;
	};

	struct DeviceInfo {
		uint32_t apiVersion;
		uint32_t queueFamilyIndex;
		void* queueHandle;
		void* descriptorPool;
		uint32_t descriptorPoolSize;
		int minImageCount;
		size_t imageCount;
	};

	struct PipelineInfo {
		void* renderPass;
		uint32_t subpass;
		uint32_t pipelineFlags;
		uint32_t usageFlags;
	};


public:
	virtual ~RenderDevice() = default;

	RenderDevice(const RenderDevice& other) = delete;
	RenderDevice(const RenderDevice&& other) = delete;
	RenderDevice& operator=(const RenderDevice& other) = delete;
	RenderDevice& operator=(const RenderDevice&& other) = delete;

	virtual int init(WindowConfig platform) override = 0;
	virtual int onClose() override = 0;
	virtual void onUpdate() override = 0;
	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;
	virtual void render() = 0;

	virtual void* getNativeInstance() = 0;
	virtual void* getNativeDevice() = 0;
	virtual void* getPhysicalDevice() = 0;
	virtual CommandBufferHandle* getCommandBuffer(int id) = 0;
	virtual void* getNativeRenderPass(int id) = 0;
	virtual DeviceInfo getDeviceInfo(int id) = 0;
	virtual PipelineInfo getPipelineInfo(int id) = 0;


protected:
	RenderDevice(std::string name = "RenderDevice") : Service(name) {};
	

private:


};

