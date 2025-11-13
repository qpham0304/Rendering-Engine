#include "AppWindow.h"
#include "camera.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/Event.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/core/scene/SceneManager.h"
#include "../../src/window/Input.h"

const std::set<RenderPlatform> AppWindow::supportRenderPlatform = { RenderPlatform::OPENGL, RenderPlatform::VULKAN };
AppWindow* AppWindow::window = nullptr;


AppWindow::AppWindow(std::string serviceName)
	: Service(serviceName),
	m_width(0),
	m_height(0),
	m_input(nullptr)
{
}

const WindowConfig& AppWindow::getWindowConfig()
{
	return window->m_config;
}

bool AppWindow::isMousePressed(MouseCodes mouseCode) {
	return window->m_input->isMousePressed(mouseCode);
}

bool AppWindow::isKeyPressed(KeyCodes keyCode) {
	return window->m_input->isKeyPressed(keyCode);
}

int AppWindow::getMouseButton(MouseCodes mouseCode) {
	return window->m_input->getMouseButton(mouseCode);
}

void AppWindow::getCursorPos(double* x, double* y) {
	return window->m_input->getCursorPos(x, y);
}

int AppWindow::getKey(KeyCodes keyCode) {
	return window->m_input->getKey(keyCode);
}

double AppWindow::getTime()
{
	return window->_getTime();
}

unsigned int AppWindow::getWidth()
{
	return window->m_width;
}

unsigned int AppWindow::getHeight()
{
	return window->m_height;
}

void* AppWindow::getWindowHandle()
{
	return window->_getWindow();
}

void AppWindow::setContext(AppWindow* other)
{
	window = other;
}

void AppWindow::createWindowSurface(void* instance, void* surface)
{
	window->_createWindowSurface(instance, surface);
}
