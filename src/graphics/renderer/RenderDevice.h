#pragma once

class RenderDevice
{
public:
	RenderDevice() = default;
	virtual ~RenderDevice() = default;

	RenderDevice(const RenderDevice& other) = delete;
	RenderDevice(const RenderDevice&& other) = delete;
	RenderDevice& operator=(const RenderDevice& other) = delete;
	RenderDevice& operator=(const RenderDevice&& other) = delete;

protected:

private:
};

