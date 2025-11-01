#include "AppWindow.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/Event.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/core/scene/SceneManager.h"
#include "camera.h"
#include "../../src/window/Input.h"

unsigned int AppWindow::width = DEFAULT_WIDTH;
unsigned int AppWindow::height = DEFAULT_HEIGHT;
bool AppWindow::VsyncEnabled = false;

RenderPlatform AppWindow::platform = RenderPlatform::UNDEFINED;
const std::set<RenderPlatform> AppWindow::supportRenderPlatform = { RenderPlatform::OPENGL };

//AppWindow* AppWindow::window = nullptr;
//AppWindow* AppWindow::sharedWindow = nullptr;
AppWindow* AppWindow::window = nullptr;
