#pragma once

#include "../../AppWindow.h"

class GLFWwindow;

class AppWindowGLFW : public AppWindow
{
private:
	GLFWwindow* m_windowHandle;
	GLFWwindow* m_sharedWindowHandle;

public:
	AppWindowGLFW();
	~AppWindowGLFW();


	virtual int init(WindowConfig platform) override;		// set up and init the graphics api depending on the platform
	virtual int onClose() override;						// close and terminate the program
	virtual void onUpdate() override;

protected:
	void* _getWindow() override;
	void* _getSharedWindow() override;
	virtual double _getTime() const override;
	virtual void _setEventCallback() override;
	virtual void _createWindowSurface(void* instance, void* surface) override;

	//platform specific implementations
	//Ideally want to have glu files each platform
	//but no more than 3 graphics and 3 window api
	//are supported so these are enough
	int _initOpenGL();
	void _onCloseOpenGL();
	void _onUpdateOpenGL();

	int _initVulkan();
	void _onCloseVulkan();
	void _onUpdateVulkan();

	void _createSurfaceVulkan(void* instance, void* surface);
};

