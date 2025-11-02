#pragma once

#include <set>
#include <string>
#include "../../src/core/features/Configs.h"

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

	WindowConfig getWindowConfig() const;

	unsigned int width;
	unsigned int height;

protected:
	
	AppWindow() = default;


	virtual void setEventCallback() = 0;
	
	WindowConfig config;
};