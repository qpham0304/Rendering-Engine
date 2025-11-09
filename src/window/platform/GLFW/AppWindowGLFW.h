#pragma once

#include "../../AppWindow.h"

class GLFWwindow;

class AppWindowGLFW : public AppWindow
{
private:
	GLFWwindow* m_WindowHandle;
	GLFWwindow* m_SharedWindowHandle;

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
};

