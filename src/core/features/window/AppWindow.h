#pragma once

#include <set>
#include "../../src/gui/GuiController.h"

enum Platform {
	PLATFORM_UNDEFINED, PLATFORM_OPENGL, PLATFORM_VULKAN, PLATFORM_DIRECTX,
};

class AppWindow
{
protected:
	AppWindow() = default;
	~AppWindow() = default;

private:
	static const unsigned int DEFAULT_WIDTH = 720;
	static const unsigned int DEFAULT_HEIGHT = 1280;
	static void setEventCallback();

public:
	static bool VsyncEnabled;
	static unsigned int width;
	static unsigned int height;
	static Platform platform;
	static const std::set<Platform> supportPlatform;
	static ImGuizmo::OPERATION GuizmoType;
	static GLFWwindow* window;
	static GLFWwindow* sharedWindow;

	static int init(Platform platform);		// set up and init the graphics api depending on the platform
	static int start(const char* title);	// start creating windows and context
	static int end();						// close and terminate the program
	static void onUpdate();
};

