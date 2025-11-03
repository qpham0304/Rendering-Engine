#pragma once

#include <memory>
#include <set>
#include <string>
#include "../../src/core/features/Configs.h"
#include "Input.h"

class GLFWwindow;

class AppWindow
{
public:
	static AppWindow* window;
	~AppWindow() = default;

	static const std::set<RenderPlatform> supportRenderPlatform;
	static bool VsyncEnabled;
	static RenderPlatform platform;

	virtual GLFWwindow* getWindow() = 0;
	virtual GLFWwindow* getSharedWindow() = 0;

	virtual int init(WindowConfig config) = 0;	// set up and init the graphics api depending on the platform
	virtual int start() = 0;					// start creating windows and context
	virtual int end() = 0;						// close and terminate the program
	virtual void onUpdate() = 0;
	virtual double getTime() = 0;

	WindowConfig getWindowConfig() const;
	bool isMousePressed(MouseCodes mouseCode);
	bool isKeyPressed(KeyCodes keyCode);
	int getMouseButton(MouseCodes mouseCode);
	void getCursorPos(double* x, double* y);
	int getKey(KeyCodes keyCode);

	unsigned int width;
	unsigned int height;

protected:
	AppWindow() = default;

	std::unique_ptr<Input> input;

	virtual void setEventCallback() = 0;
	
	WindowConfig config;
};