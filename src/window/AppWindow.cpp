#include "AppWindow.h"
#include "camera.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/Event.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/core/scene/SceneManager.h"
#include "../../src/window/Input.h"

bool AppWindow::VsyncEnabled = false;

RenderPlatform AppWindow::platform = RenderPlatform::UNDEFINED;
const std::set<RenderPlatform> AppWindow::supportRenderPlatform = { RenderPlatform::OPENGL };

AppWindow* AppWindow::window = nullptr;


WindowConfig AppWindow::getWindowConfig() const
{
	return config;
}

bool AppWindow::isMousePressed(MouseCodes mouseCode){
	return input->isMousePressed(mouseCode);
}

bool AppWindow::isKeyPressed(KeyCodes keyCode) {
	return input->isKeyPressed(keyCode);
}

int AppWindow::getMouseButton(MouseCodes mouseCode) {
	return input->getMouseButton(mouseCode);
}

void AppWindow::getCursorPos(double* x, double* y) {
	return input->getCursorPos(x, y);
}

int AppWindow::getKey(KeyCodes keyCode) {
	return input->getKey(keyCode);
}