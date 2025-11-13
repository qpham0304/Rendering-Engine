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

protected:
	Logger& Log() const;

private:
	Logger* m_logger;
};

