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

	void* getWindow() override;
	void* getSharedWindow() override;


	virtual int init(WindowConfig platform) override;		// set up and init the graphics api depending on the platform
	virtual int onClose() override;						// close and terminate the program
	virtual void onUpdate() override;
	virtual double getTime() const override;

protected:
	virtual void setEventCallback() override;
};

