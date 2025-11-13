#pragma once

#include "../../src/graphics/RenderDevice.h"

#include <memory>

#include <vulkan/vulkan.h>

class Logger;

class RenderDeviceVulkan : public RenderDevice
{

public:
	RenderDeviceVulkan();
	~RenderDeviceVulkan() override = default;

	virtual int init(WindowConfig platform) override;
	virtual int onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;

protected:
	Logger& Log() const;

private:
	Logger* m_logger;

};

