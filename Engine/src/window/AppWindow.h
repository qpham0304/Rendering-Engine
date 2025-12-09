#pragma once

#include <memory>
#include <set>
#include <string>
#include "Input.h"
#include "../../src/core/features/Configs.h"
#include "../../src/services/Service.h"

class GLFWwindow;

class AppWindow : public Service
{
public:
	static const std::set<RenderPlatform> supportRenderPlatform;

public:
	virtual ~AppWindow() = default;

	virtual int init(WindowConfig config) = 0;
	virtual int onClose() = 0;
	virtual void onUpdate() = 0;

	static const WindowConfig& getWindowConfig();
	static bool isMousePressed(MouseCodes mouseCode);
	static bool isKeyPressed(KeyCodes keyCode);
	static int getMouseButton(MouseCodes mouseCode);
	static void getCursorPos(double* x, double* y);
	static int getKey(KeyCodes keyCode);
	static double getTime();
	static unsigned int getWidth();
	static unsigned int getHeight();
	static void* getWindowHandle();
	static void* getNativeWindowHandle();
	static void getFrameBufferSize(int* width, int* height);
	static void waitEvents();
	static void setContextCurrent();

protected:
	AppWindow(std::string serviceName = "AppWindow");
	
	virtual void* _getWindow() = 0;
	virtual void* _getSharedWindow() = 0;
	virtual void* _getNativeWindowHandle() = 0;
	virtual void _setEventCallback() = 0;
	virtual double _getTime() const = 0;
	virtual void _getFrameBufferSize(int& width, int& height) = 0;
	virtual void _waitEvents() = 0;
	virtual void _setContextCurrent() = 0;

protected:
	static AppWindow* window;

	std::unique_ptr<Input> m_input;
	
	unsigned int m_width;
	unsigned int m_height;
};