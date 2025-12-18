#pragma once

#include "window/AppWindow.h"

class GLFWwindow;

class AppWindowGLFW : public AppWindow
{
private:
	GLFWwindow* m_windowHandle;
	GLFWwindow* m_sharedWindowHandle;

public:
	AppWindowGLFW();
	~AppWindowGLFW();


	virtual bool init(WindowConfig platform) override;		// set up and init the graphics api depending on the platform
	virtual void onUpdate() override;
	virtual bool onClose() override;							// close and terminate the program

protected:
	void* _getWindow() override;
	void* _getSharedWindow() override;
	virtual void* _getNativeWindowHandle() override;
	virtual double _getTime() const override;
	virtual void _setEventCallback() override;
	void _getFrameBufferSize(int& width, int& height) override;
	void _waitEvents() override;
	void _setContextCurrent() override;

	//platform specific implementations
	//Ideally want to have glue files for each platform
	//but no more than 3 graphics and 3 window api
	//are supported so these are enough
	bool _initOpenGL();
	bool _onCloseOpenGL();
	void _onUpdateOpenGL();

	bool _initVulkan();
	bool _onCloseVulkan();
	void _onUpdateVulkan();

};

