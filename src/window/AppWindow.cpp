#include "AppWindow.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/Event.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/core/scene/SceneManager.h"
#include "camera.h"
#include "../../src/window/Input.h"

bool AppWindow::VsyncEnabled = false;

RenderPlatform AppWindow::platform = RenderPlatform::UNDEFINED;
const std::set<RenderPlatform> AppWindow::supportRenderPlatform = { RenderPlatform::OPENGL };

AppWindow* AppWindow::window = nullptr;


WindowConfig AppWindow::getWindowConfig() const
{
	return config;
}