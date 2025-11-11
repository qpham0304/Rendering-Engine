#include "AppWindow.h"
#include "camera.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/Event.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/core/scene/SceneManager.h"
#include "../../src/window/Input.h"

RenderPlatform AppWindow::platform = RenderPlatform::UNDEFINED;
const std::set<RenderPlatform> AppWindow::supportRenderPlatform = { RenderPlatform::OPENGL, RenderPlatform::VULKAN };
AppWindow* AppWindow::window = nullptr;


AppWindow::AppWindow(std::string serviceName)
	: Service(serviceName),
	width(0),
	height(0),
	config(),
	input(nullptr)
{
}

const WindowConfig& AppWindow::getWindowConfig()
{
	return window->config;
}

bool AppWindow::isMousePressed(MouseCodes mouseCode) {
	return window->input->isMousePressed(mouseCode);
}

bool AppWindow::isKeyPressed(KeyCodes keyCode) {
	return window->input->isKeyPressed(keyCode);
}

int AppWindow::getMouseButton(MouseCodes mouseCode) {
	return window->input->getMouseButton(mouseCode);
}

void AppWindow::getCursorPos(double* x, double* y) {
	return window->input->getCursorPos(x, y);
}

int AppWindow::getKey(KeyCodes keyCode) {
	return window->input->getKey(keyCode);
}

double AppWindow::getTime()
{
	return window->_getTime();
}

unsigned int AppWindow::getWidth()
{
	return window->width;
}

unsigned int AppWindow::getHeight()
{
	return window->height;
}

void* AppWindow::getWindowHandle()
{
	return window->_getWindow();
}

void AppWindow::setContext(AppWindow* other)
{
	window = other;
}
