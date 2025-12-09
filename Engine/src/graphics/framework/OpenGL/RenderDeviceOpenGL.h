#pragma once

#include "../../src/graphics/RenderDevice.h"
#include <memory>

class Logger;

class RenderDeviceOpenGL : public RenderDevice
{
public:
	RenderDeviceOpenGL();
	~RenderDeviceOpenGL() override = default;

	virtual int init(WindowConfig platform) override;
	virtual int onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void render() override;
	virtual void endFrame() override;
	virtual void draw(uint32_t indicies = 0, uint32_t numInstances = 0) override;


	virtual void* getNativeInstance() override { return nullptr; };
	virtual void* getNativeDevice() override { return nullptr; };
	virtual void* getPhysicalDevice() override { return nullptr; };
	virtual CommandBufferHandle* getCommandBuffer(int id) override { return nullptr; };
	virtual void* getNativeRenderPass(int id) override { return nullptr; };
	virtual DeviceInfo getDeviceInfo() const override { return DeviceInfo(); };
	virtual PipelineInfo getPipelineInfo() const override { return PipelineInfo(); };

protected:
	Logger& Log() const;

private:
	Logger* m_logger;
};

