#include "AppWindowSDL.h"
#include "core/features/EngineStates.h"
#include "core/features/Timer.h"
#include "core/events/eventManager.h"
#include "InputSDL.h"
#include <SDL3/SDL.h>
#include <stdexcept>

AppWindowSDL::AppWindowSDL()
	: AppWindow(), m_windowHandle(nullptr), m_minimized(false)
{
	window = &*this;
	m_input = std::make_unique<InputSDL>();
}


AppWindowSDL::~AppWindowSDL()
{
	m_input.reset();
}


bool AppWindowSDL::init(WindowConfig config)
{
	Service::init(config);

	m_width = m_config.width;
	m_height = m_config.height;

	if (supportRenderPlatform.find(m_config.renderPlatform) == supportRenderPlatform.end()) {
		throw std::runtime_error("AppWindowSDL init: Platform unsupported");
	}

	switch (m_config.renderPlatform) {
		case RenderPlatform::VULKAN: _initVulkan(); break;
		default: throw std::runtime_error("AppWindowSDL init: only Vulkan is supported with SDL"); break;
	}

	_setEventCallback();

	InputSDL* inputHandle = dynamic_cast<InputSDL*>(m_input.get());
	if (!inputHandle) {
		throw std::runtime_error("AppWindowSDL init: failed to cast Input to type InputSDL");
	}
	inputHandle->m_windowHandle = m_windowHandle;

	return true;
}


bool AppWindowSDL::onClose() {
	switch (m_config.renderPlatform) {
		case RenderPlatform::VULKAN: _onCloseVulkan(); break;
		default: throw std::runtime_error("Platform not supported for onClose"); break;
	}
	return true;
}


void AppWindowSDL::onUpdate()
{
	//TODO: deltatime ideally should be passed through system's update not from AppWindow
	double currentTime = _getTime();
	m_deltaTime = 0.0;

	if(EngineState::isPlaying()) {
		if(EngineState::didJustResume()) {
			m_deltaTime = 0.0;
		} else {
			m_deltaTime = currentTime - m_lastTime;
		}
	} else {
		m_deltaTime = 0.0;
	}
	m_lastTime = currentTime;

	switch (m_config.renderPlatform) {
		case RenderPlatform::VULKAN: _onUpdateVulkan(); break;
		default: throw std::runtime_error("Platform not supported for onUpdate"); break;
	}
}

// pixels, which is what the swapchain needs. reports 0 x 0 while minimized like GLFW does,
// the swapchain waits on that before recreating
void AppWindowSDL::_getFrameBufferSize(int& width, int& height)
{
	if (m_minimized || (SDL_GetWindowFlags(m_windowHandle) & SDL_WINDOW_MINIMIZED)) {
		width = 0;
		height = 0;
		return;
	}
	SDL_GetWindowSizeInPixels(m_windowHandle, &width, &height);
}

void AppWindowSDL::_waitEvents()
{
	SDL_Event event;
	if (SDL_WaitEvent(&event)) {
		_handleEvent(event);
	}
}

// OpenGL only
void AppWindowSDL::_setContextCurrent()
{
}

void AppWindowSDL::_setFullscreen(bool fullscreen, bool borderless) {
	if (fullscreen) {
		SDL_DisplayID display = SDL_GetDisplayForWindow(m_windowHandle);
		const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(display);

		if (borderless) {
			// borderless mode, covers the screen without changing the display mode
			SDL_SetWindowFullscreenMode(m_windowHandle, nullptr);
		}
		else {
			// true fullscreen mode this actually changes the monitor resolution
			SDL_SetWindowFullscreenMode(m_windowHandle, mode);
		}
		SDL_SetWindowFullscreen(m_windowHandle, true);

		if (mode) {
			m_width = mode->w;
			m_height = mode->h;
		}
	}
	else {
		// windowed mode
		SDL_SetWindowFullscreen(m_windowHandle, false);
		SDL_SetWindowSize(m_windowHandle, m_config.width, m_config.height);
		SDL_SetWindowPosition(m_windowHandle, 100, 100);
	}
}

void* AppWindowSDL::_getWindow()
{
	return m_windowHandle;
}


void* AppWindowSDL::_getSharedWindow()
{
	throw std::runtime_error("Shared window is only available for OpenGL platform");
}

void* AppWindowSDL::_getNativeWindowHandle()
{
	void* handle = nullptr;
#if defined(_WIN32)
	handle = SDL_GetPointerProperty(SDL_GetWindowProperties(m_windowHandle), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
	// iOS / macOS: the Vulkan surface should come from SDL_Vulkan_CreateSurface instead of a native handle
#endif
	return handle;
}


double AppWindowSDL::_getTime() const
{
	return static_cast<double>(SDL_GetTicksNS()) / 1e9;
}

double AppWindowSDL::_getDeltaTime() const
{
	return m_deltaTime;
}

// nothing to register: SDL delivers events through the poll loop, _handleEvent publishes them
void AppWindowSDL::_setEventCallback()
{
}

void AppWindowSDL::_handleEvent(const SDL_Event& event)
{
	// the gui backend (ImGui) needs every event, including the ones for its own popout windows
	if (m_nativeEventCallback) {
		m_nativeEventCallback((void*)&event);
	}

	SDL_WindowID mainWindowID = SDL_GetWindowID(m_windowHandle);
	InputSDL* input = static_cast<InputSDL*>(m_input.get());

	switch (event.type) {
		case SDL_EVENT_QUIT: {
			WindowCloseEvent closeEvent;
			EventManager::getInstance().publish(closeEvent);
			break;
		}

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
			// imgui popout windows send this too, only the main window closes the app
			if (event.window.windowID == mainWindowID) {
				WindowCloseEvent closeEvent;
				EventManager::getInstance().publish(closeEvent);
			}
			break;
		}

		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
			if (event.window.windowID == mainWindowID && !m_minimized) {
				m_width = event.window.data1;
				m_height = event.window.data2;
				WindowResizeEvent resizeEvent(event.window.data1, event.window.data2);
				EventManager::getInstance().publish(resizeEvent);
			}
			break;
		}

		case SDL_EVENT_WINDOW_MINIMIZED: {
			if (event.window.windowID == mainWindowID) {
				m_minimized = true;
				WindowResizeEvent resizeEvent(0, 0);
				EventManager::getInstance().publish(resizeEvent);
			}
			break;
		}

		case SDL_EVENT_WINDOW_RESTORED:
		case SDL_EVENT_WINDOW_MAXIMIZED: {
			// coming back from minimized does not always change the pixel size, so announce it here
			if (event.window.windowID == mainWindowID && m_minimized) {
				m_minimized = false;
				int width = 0, height = 0;
				SDL_GetWindowSizeInPixels(m_windowHandle, &width, &height);
				m_width = width;
				m_height = height;
				WindowResizeEvent resizeEvent(width, height);
				EventManager::getInstance().publish(resizeEvent);
			}
			break;
		}

		case SDL_EVENT_KEY_DOWN: {
			// GLFW only reports the first press, not the auto repeat
			if (event.key.windowID == mainWindowID && !event.key.repeat) {
				int key = InputSDL::toEngineKey(event.key.scancode);
				if (key >= 0) {
					KeyPressedEvent keyPressEvent(key);
					EventManager::getInstance().publish(keyPressEvent);
				}
			}
			break;
		}

		case SDL_EVENT_MOUSE_MOTION: {
			if (event.motion.windowID == mainWindowID) {
				Timer timer("cursor event", true);
				if (input->m_relativeMode) {
					input->m_cursorX += event.motion.xrel;
					input->m_cursorY += event.motion.yrel;
				} else {
					input->m_cursorX = event.motion.x;
					input->m_cursorY = event.motion.y;
				}
				MouseMoveEvent cursorMoveEvent(input->m_cursorX, input->m_cursorY);
				EventManager::getInstance().publish(cursorMoveEvent);
			}
			break;
		}

		case SDL_EVENT_MOUSE_WHEEL: {
			if (event.wheel.windowID == mainWindowID) {
				Timer timer("scroll event", true);
				MouseScrollEvent scrollEvent(event.wheel.x, event.wheel.y);
				EventManager::getInstance().publish(scrollEvent);
			}
			break;
		}

		default:
			break;
	}
}


/*
	Vulkan specfic implementations
*/
#pragma region VULKAN
bool AppWindowSDL::_initVulkan()
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		throw std::runtime_error(std::string("AppWindowSDL: SDL_Init failed: ") + SDL_GetError());
	}

	SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	m_windowHandle = SDL_CreateWindow(m_config.title.c_str(), m_width, m_height, flags);
	if (!m_windowHandle) {
		throw std::runtime_error(std::string("AppWindowSDL: failed to create window: ") + SDL_GetError());
	}

	int width = 0, height = 0;
	SDL_GetWindowSizeInPixels(m_windowHandle, &width, &height);
	m_width = width;
	m_height = height;

	return true;
}

bool AppWindowSDL::_onCloseVulkan()
{
	SDL_DestroyWindow(m_windowHandle);
	m_windowHandle = nullptr;
	SDL_Quit();

	return true;
}

void AppWindowSDL::_onUpdateVulkan()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		_handleEvent(event);
	}
}
#pragma endregion VULKAN
