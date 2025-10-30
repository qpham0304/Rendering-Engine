#pragma once

#include "camera.h" // lots of dependencies here
#include <Animation.h>
#include <FrameBuffer.h>
#include <DepthMap.h>
#include <DepthCubeMap.h>
#include <imgui_internal.h>
#include <set>
#include <future>
#include <mutex>
#include <thread>
#include "../../src/gui/GuiController.h"
#include "../../src/graphics/utils/Utils.h"
#include "../../src/core/components/legacy/Component.h"
#include "../../src/graphics/renderer/SkyboxRenderer.h"
#include "../../src/core/components/legacy/LightComponent.h"

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

