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
	virtual int onClose();
	virtual void onUpdate();
	virtual void beginFrame();
	virtual void endFrame();
	virtual void render() override;

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

