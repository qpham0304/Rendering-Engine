#include "AppWindow.h"
#include "core/features/Timer.h"
#include "core/events/Event.h"
#include "core/events/EventManager.h"
#include "core/scene/SceneManager.h"
#include "window/Input.h"
#include <chrono>
#include <thread>

const std::set<RenderPlatform> AppWindow::supportRenderPlatform = { RenderPlatform::OPENGL, RenderPlatform::VULKAN };
AppWindow* AppWindow::window = nullptr;


AppWindow::AppWindow(std::string serviceName)
	: Service(serviceName),
	m_width(0),
	m_height(0),
	m_input(nullptr)
{
	setFootprint("AppWindow", sizeof(AppWindow), MemorySystem::Window);
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

void AppWindow::enableCursor() {
	return window->m_input->enableCursor();
}

void AppWindow::disableCursor() {
	return window->m_input->disableCursor();
}

int AppWindow::getKey(KeyCodes keyCode) {
	return window->m_input->getKey(keyCode);
}

double AppWindow::getTime()
{
	return window->_getTime();
}

void AppWindow::sleepUntil(double time)
{
	window->_sleepUntil(time);
}

// portable fallback for backends without a precise delay, sleep most of the wait and spin the rest
// since a plain sleep can overshoot by a whole scheduler tick on some platforms
void AppWindow::_sleepUntil(double time)
{
	const double spinWindow = 0.0015;
	double remaining = time - _getTime();
	if (remaining > spinWindow) {
		std::this_thread::sleep_for(std::chrono::duration<double>(remaining - spinWindow));
	}
	while (_getTime() < time) {
		std::this_thread::yield();
	}
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

void* AppWindow::getNativeWindowHandle()
{
	return window->_getNativeWindowHandle();
}

void AppWindow::getFrameBufferSize(int* width, int* height)
{
	window->_getFrameBufferSize(*width, *height);
}

void AppWindow::waitEvents()
{
	window->_waitEvents();
}

void AppWindow::setContextCurrent()
{
	window->_setContextCurrent();
}

void AppWindow::setFullscreen(bool fullscreen, bool borderless)
{
	window->_setFullscreen(fullscreen, borderless);
}

// receives every raw platform event (an SDL_Event* with SDL). GLFW backends install their own callbacks and never call it
void AppWindow::setNativeEventCallback(std::function<void(void* nativeEvent)> callback)
{
	window->m_nativeEventCallback = std::move(callback);
}
