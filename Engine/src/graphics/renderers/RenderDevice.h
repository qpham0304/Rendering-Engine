#pragma once

#include "services/Service.h"
#include <optional>
#include <atomic>

class RenderDevice : public Service
{
public:

	struct CommandBufferHandle {
		void* commandBuffer;
	};

	struct DeviceInfo {
		std::optional<uint32_t> apiVersion;			// Can be empty or not set
		std::optional<uint32_t> queueFamilyIndex;
		std::optional<void*> queueHandle;
		std::optional<int> minImageCount;
		std::optional<size_t> imageCount;
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
	virtual void draw(uint32_t indicies = 0, uint32_t numInstances = 0) = 0;

	virtual void* getNativeInstance() = 0;
	virtual void* getNativeDevice() = 0;
	virtual void* getPhysicalDevice() = 0;
	virtual CommandBufferHandle* getCommandBuffer(int id) = 0;
	virtual void* getNativeRenderPass(int id) = 0;
	virtual DeviceInfo getDeviceInfo() const = 0;
	virtual PipelineInfo getPipelineInfo() const = 0;


protected:
	RenderDevice(std::string name = "RenderDevice") : Service(name) {};


protected:
	std::atomic<int> id{ 0 };

private:

};

