#include "AppWindowGLFW.h"
#include <glad/glad.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/eventManager.h"
#include "InputGLFW.h"

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


int AppWindowGLFW::init(WindowConfig config) 
{
	Service::init(config);

	m_width = m_config.width;
	m_height = m_config.height;

	if (supportRenderPlatform.find(m_config.renderPlatform) == supportRenderPlatform.end()) {
		throw std::runtime_error("Platform unsupported");
	}

	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL: _initOpenGL(); break;
		case RenderPlatform::VULKAN: _initVulkan(); break;
		default: throw std::runtime_error("Render platform not supported"); break;
	}

	_setEventCallback();

	InputGLFW* inputHandle = dynamic_cast<InputGLFW*>(m_input.get());
	if (!inputHandle) {
		throw std::runtime_error("AppWindowGLFW: failed to cast Input to type InputGLFW");
	}
	inputHandle->m_windowHandle = m_windowHandle;

	glfwSwapInterval(m_config.vsync);

	return 0;
}


int AppWindowGLFW::onClose() {
	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL: _onCloseOpenGL(); break;
		case RenderPlatform::VULKAN: _onCloseVulkan(); break;
		default: throw std::runtime_error("Platform not supported for onClose"); break;
	}
	return 0;
}


void AppWindowGLFW::onUpdate()
{
	switch (m_config.renderPlatform) {
		case RenderPlatform::OPENGL: _onUpdateOpenGL(); break;
		case RenderPlatform::VULKAN: _onUpdateVulkan(); break;
		default: throw std::runtime_error("Platform not supported for onUpdate"); break;
	}
}

void AppWindowGLFW::_createWindowSurface(void* instance, void* surface)
{
	switch (m_config.renderPlatform) {
	case RenderPlatform::OPENGL: throw std::runtime_error("No support for OpenGL"); break;
	case RenderPlatform::VULKAN: _createSurfaceVulkan(instance, surface); break;

	}
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
				case GLFW_PRESS: {
					if (key == KEY_ESCAPE) {
						WindowCloseEvent windowCloseEvent;
						EventManager::getInstance().publish(windowCloseEvent);
					}
					else {
						KeyPressedEvent keyPressEvent(key);
						EventManager::getInstance().publish(keyPressEvent);
					}
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

int AppWindowGLFW::_initOpenGL()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Initialize GLFW
	if (!glfwInit())
		return -1;

	// Get primary monitor
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	if (!monitor) {
		throw std::runtime_error("Failed to get primary monitor");
		glfwTerminate();
		return -1;
	}

	// Get monitor video mode
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	if (!mode) {
		throw std::runtime_error("Failed to get video mode");
		glfwTerminate();
		return -1;
	}

	// force window to always proportional to what every user's screen has
	m_width = mode->width * 2 / 3;
	m_height = mode->height * 2 / 3;

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);	// Set Invisible for the second context
	m_sharedWindowHandle = glfwCreateWindow(m_width, m_height, "", NULL, m_windowHandle);

	if (m_sharedWindowHandle == NULL) {
		throw std::runtime_error("Failed to create shared GLFW window");
		glfwTerminate();
		return -1;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	//glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);	// disable title bar

	m_windowHandle = glfwCreateWindow(m_width, m_height, m_config.title.c_str(), NULL, NULL);

	if (m_windowHandle == NULL) {
		throw std::runtime_error("Failed to create GLFW window");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(m_windowHandle);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		throw std::runtime_error("Failed to initialize GLAD for main context");
		glfwDestroyWindow(m_windowHandle);
		glfwTerminate();
		return -1;
	}
}

void AppWindowGLFW::_onCloseOpenGL()
{
	glfwMakeContextCurrent(nullptr);
	glfwDestroyWindow(m_windowHandle);
	glfwDestroyWindow(m_sharedWindowHandle);
	glfwTerminate();
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

int AppWindowGLFW::_initVulkan()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_windowHandle = glfwCreateWindow(m_width, m_height, m_config.title.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(m_windowHandle, this);

	return 0;
}

void AppWindowGLFW::_onCloseVulkan()
{
	glfwDestroyWindow(m_windowHandle);
	glfwTerminate();
}

void AppWindowGLFW::_onUpdateVulkan()
{
	glfwPollEvents();

	//throw std::runtime_error("Vulkan onUpdate not yet implemented");
}

void AppWindowGLFW::_createSurfaceVulkan(void* instancePtr, void* surfacePtr)
{
	VkInstance instance = reinterpret_cast<VkInstance>(instancePtr);
	VkSurfaceKHR* surface = reinterpret_cast<VkSurfaceKHR*>(surfacePtr);

	if (glfwCreateWindowSurface(instance, m_windowHandle, nullptr, surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}
