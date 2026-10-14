#pragma once

#include "window/AppWindow.h"

struct SDL_Window;
union SDL_Event;

class AppWindowSDL : public AppWindow
{
private:
	SDL_Window* m_windowHandle;
	bool m_minimized;

public:
	AppWindowSDL();
	~AppWindowSDL();


	virtual bool init(WindowConfig platform) override;
	virtual void onUpdate() override;
	virtual bool onClose() override;

protected:
	void* _getWindow() override;
	void* _getSharedWindow() override;
	virtual void* _getNativeWindowHandle() override;
	virtual double _getTime() const override;
	virtual double _getDeltaTime() const override;
	virtual void _setEventCallback() override;
	void _getFrameBufferSize(int& width, int& height) override;
	void _waitEvents() override;
	void _setContextCurrent() override;
	void _setFullscreen(bool fullscreen, bool borderless) override;

	// GLFW calls back per event, SDL hands every event to this from the poll loop
	void _handleEvent(const SDL_Event& event);

	//only Vulkan is supported through SDL for now
	bool _initVulkan();
	bool _onCloseVulkan();
	void _onUpdateVulkan();
};
