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
	static const std::set<RenderPlatform> supportRenderPlatform;
	static bool VsyncEnabled;
	static RenderPlatform platform;

	static AppWindow* window;
	unsigned int width;
	unsigned int height;

public:
	virtual ~AppWindow() = default;

	virtual void* getWindow() = 0;
	virtual void* getSharedWindow() = 0;

	virtual int init(WindowConfig config) = 0;	// set up and init the graphics api depending on the platform
	virtual int onClose() = 0;						// close and terminate the program
	virtual void onUpdate() = 0;
	virtual double getTime() const = 0;

	const WindowConfig& getWindowConfig() const;
	bool isMousePressed(MouseCodes mouseCode) const;
	bool isKeyPressed(KeyCodes keyCode) const;
	int getMouseButton(MouseCodes mouseCode) const;
	void getCursorPos(double* x, double* y);
	int getKey(KeyCodes keyCode) const;


protected:
	AppWindow() = default;

	std::unique_ptr<Input> input;
	WindowConfig config;

	
	virtual void setEventCallback() = 0;
};