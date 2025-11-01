#pragma once
#include "../../AppWindow.h"

class GLFWwindow;

class AppWindowGLFW : public AppWindow
{
private:
	GLFWwindow* window;
	GLFWwindow* sharedWindow;

public:
	AppWindowGLFW();
	~AppWindowGLFW();

	GLFWwindow* getWindow() override;
	GLFWwindow* getSharedWindow() override;

	virtual int init(WindowConfig platform) override;		// set up and init the graphics api depending on the platform
	virtual int start() override;	// start creating windows and context
	virtual int end() override;						// close and terminate the program
	virtual void onUpdate() override;

protected:
	virtual void setEventCallback() override;
};

