#include "AppWindowGLFW.h"
#include <glad/glad.h>
#define GLFW_INCLUDE_VULKAN
#include "InputGLFW.h"
#include <GLFW/glfw3.h>
#if defined _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "core/features/Timer.h"
#include "core/events/eventManager.h"
#endif

//extern "C" __declspec(dllexport)
//AppWindow* CreateAppWindowGLFW()
//{
//	return new AppWindowGLFW();
//}

AppWindowGLFW::AppWindowGLFW() 
	: AppWindow(), m_windowHandle(nullptr), m_sharedWindowHandle(nullptr)
{
	window = &*this;
	m_input = std::make_unique<InputGLFW>();
}


AppWindowGLFW::~AppWindowGLFW()
{
	m_input.reset();
}


bool AppWindowGLFW::init(WindowConfig config) 
{
	Service::init(config);

	m_width = m_config.width;
	m_height = m_config.height;

	if (supportRenderPlatform.find(m_config.renderPlatform) == supportRenderPlatform.end()) {
		throw std::runtime_error("AppWindowGLFW init: Platform unsupported");
	}

	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL: _initOpenGL(); break;
		case RenderPlatform::VULKAN: _initVulkan(); break;
		default: throw std::runtime_error("Render platform not supported"); break;
	}

	_setEventCallback();

	InputGLFW* inputHandle = dynamic_cast<InputGLFW*>(m_input.get());
	if (!inputHandle) {
		throw std::runtime_error("AppWindowGLFW init: failed to cast Input to type InputGLFW");
	}
	inputHandle->m_windowHandle = m_windowHandle;

	glfwSwapInterval(m_config.vsync);

	return true;
}


bool AppWindowGLFW::onClose() {
	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL: _onCloseOpenGL(); break;
		case RenderPlatform::VULKAN: _onCloseVulkan(); break;
		default: throw std::runtime_error("Platform not supported for onClose"); break;
	}
	return true;
}


void AppWindowGLFW::onUpdate()
{
	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL: _onUpdateOpenGL(); break;
		case RenderPlatform::VULKAN: _onUpdateVulkan(); break;
		default: throw std::runtime_error("Platform not supported for onUpdate"); break;
	}
}
void AppWindowGLFW::_getFrameBufferSize(int& width, int& height)
{
	glfwGetFramebufferSize(m_windowHandle, &width, &height);	//TODO: remove abstract to appwindow
}

void AppWindowGLFW::_waitEvents()
{
	glfwWaitEvents();
}

void AppWindowGLFW::_setContextCurrent()
{
	glfwMakeContextCurrent(m_windowHandle);
}


void* AppWindowGLFW::_getWindow()
{
	return m_windowHandle;
}


void* AppWindowGLFW::_getSharedWindow()
{
	if(m_config.renderPlatform != RenderPlatform::OPENGL) {
		throw std::runtime_error("Shared window is only available for OpenGL platform");
	}
	return m_sharedWindowHandle;
}

void* AppWindowGLFW::_getNativeWindowHandle()
{
	void* handle;
#if defined(_WIN32)
	handle = glfwGetWin32Window(m_windowHandle);
//	h.platform = PlatformType::Win32;
//	h.window = glfwGetWin32Window(m_window);
//	h.display = nullptr;  // not needed on Windows
#elif defined(__linux__)
//	h.platform = PlatformType::X11;
//	h.window = (void*)glfwGetX11Window(m_window);
//	h.display = (void*)glfwGetX11Display();
#elif defined(__APPLE__)
//	h.platform = PlatformType::Cocoa;
//	h.window = glfwGetCocoaWindow(m_window);
#endif
	return handle;
}


double AppWindowGLFW::_getTime() const
{
	return glfwGetTime();
}


void AppWindowGLFW::_setEventCallback()
{
	glfwSetCursorPosCallback(m_windowHandle, [](GLFWwindow* window, double x, double y)
	{
		Timer timer("cursor event", true);
		MouseMoveEvent cursorMoveEvent(x, y);
		EventManager::getInstance().publish(cursorMoveEvent);
	});

	glfwSetScrollCallback(m_windowHandle, [](GLFWwindow* window, double x, double y)
	{
		Timer timer("scroll event", true);
		MouseScrollEvent scrollEvent(x, y);
		EventManager::getInstance().publish(scrollEvent);
		//EventManager::getInstance().publish("mouseScrollEvent", x, y);
	});

	glfwSetKeyCallback(m_windowHandle, [](GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		switch (action) {
			std::cout << "Key: " << key << ", Action: " << action << ", Mods: " << mods << std::endl;
			case GLFW_PRESS: {
				KeyPressedEvent keyPressEvent(key);
				EventManager::getInstance().publish(keyPressEvent);
			}
		}
	});

	glfwSetWindowSizeCallback(m_windowHandle, [](GLFWwindow* window, int width, int height)
	{
		WindowResizeEvent resizeEvent(width, height);
		EventManager::getInstance().publish(resizeEvent);
	});

	glfwSetWindowCloseCallback(m_windowHandle, [](GLFWwindow* window)
	{
		WindowCloseEvent closeEvent;
		EventManager::getInstance().publish(closeEvent);
	});

	//glfwSetFramebufferSizeCallback(m_windowHandle, [](GLFWwindow* window)
	//	{
	//		//WindowCloseEvent frmebufferResizeEvent;
	//		//EventManager::getInstance().publish(frmebufferResizeEvent);
	//	});

}


/*
	OpenGL specfic implementations
*/

bool AppWindowGLFW::_initOpenGL()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Initialize GLFW
	if (!glfwInit())
		return false;

	// Get primary monitor
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	if (!monitor) {
		throw std::runtime_error("Failed to get primary monitor");
		glfwTerminate();
		return false;
	}

	// Get monitor video mode
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	if (!mode) {
		throw std::runtime_error("Failed to get video mode");
		glfwTerminate();
		return false;
	}

	// force window to always proportional to what every user's screen has
	m_width = mode->width * 2 / 3;
	m_height = mode->height * 2 / 3;

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);	// Set Invisible for the second context
	m_sharedWindowHandle = glfwCreateWindow(m_width, m_height, "", NULL, m_windowHandle);

	if (m_sharedWindowHandle == NULL) {
		throw std::runtime_error("Failed to create shared GLFW window");
		glfwTerminate();
		return false;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	//glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);	// disable title bar

	m_windowHandle = glfwCreateWindow(m_width, m_height, m_config.title.c_str(), NULL, NULL);

	if (m_windowHandle == NULL) {
		throw std::runtime_error("Failed to create GLFW window");
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(m_windowHandle);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		throw std::runtime_error("Failed to initialize GLAD for main context");
		glfwDestroyWindow(m_windowHandle);
		glfwTerminate();
		return false;
	}

	return true;
}

bool AppWindowGLFW::_onCloseOpenGL()
{
	glfwMakeContextCurrent(nullptr);
	glfwDestroyWindow(m_windowHandle);
	glfwDestroyWindow(m_sharedWindowHandle);
	glfwTerminate();

	return true;
}

void AppWindowGLFW::_onUpdateOpenGL()
{
	glfwPollEvents();
	glfwSwapBuffers(m_windowHandle);
	
	// Clear background for dock space
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

/*
	Vulkan specfic implementations
*/

bool AppWindowGLFW::_initVulkan()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_windowHandle = glfwCreateWindow(m_width, m_height, m_config.title.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(m_windowHandle, this);

	return true;
}

bool AppWindowGLFW::_onCloseVulkan()
{
	glfwDestroyWindow(m_windowHandle);
	glfwTerminate();
	
	return true;
}

void AppWindowGLFW::_onUpdateVulkan()
{
	glfwPollEvents();

	//throw std::runtime_error("Vulkan onUpdate not yet implemented");
}
