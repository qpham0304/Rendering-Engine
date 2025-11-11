#include "AppWindowGLFW.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/eventManager.h"
#include "InputGLFW.h"

AppWindowGLFW::AppWindowGLFW() 
	: AppWindow(), m_WindowHandle(nullptr), m_SharedWindowHandle(nullptr)
{
	window = &*this;
	input = std::make_unique<InputGLFW>();
}


AppWindowGLFW::~AppWindowGLFW()
{
	input.reset();
}


int AppWindowGLFW::init(WindowConfig newConfig) {
	this->config = newConfig;
	width = config.width;
	height = config.height;

	platform = config.renderPlatform;
	if (supportRenderPlatform.find(config.renderPlatform) == supportRenderPlatform.end()) {
		throw std::runtime_error("Platform unsupported");
	}

	switch (platform) {
		case RenderPlatform::OPENGL: _initOpenGL(); break;
		case RenderPlatform::VULKAN: _initVulkan(); break;
		default: throw std::runtime_error("Render platform not supported"); break;
	}

	_setEventCallback();

	InputGLFW* inputHandle = dynamic_cast<InputGLFW*>(input.get());
	if (!inputHandle) {
		throw std::runtime_error("AppWindowGLFW: failed to cast Input to type InputGLFW");
	}
	inputHandle->m_WindowHandle = m_WindowHandle;

	glfwSwapInterval(config.vsync);

	return 0;
}


int AppWindowGLFW::onClose() {
	switch (platform) {
		case RenderPlatform::OPENGL: _onCloseOpenGL(); break;
		case RenderPlatform::VULKAN: _onCloseVulkan(); break;
		default: throw std::runtime_error("Platform not supported for onClose"); break;
	}
	return 0;
}


void AppWindowGLFW::onUpdate()
{
	switch (platform) {
		case RenderPlatform::OPENGL: _onUpdateOpenGL(); break;
		case RenderPlatform::VULKAN: _onUpdateVulkan(); break;
		default: throw std::runtime_error("Platform not supported for onUpdate"); break;
	}
}


void* AppWindowGLFW::_getWindow()
{
	return m_WindowHandle;
}


void* AppWindowGLFW::_getSharedWindow()
{
	if(platform != RenderPlatform::OPENGL) {
		throw std::runtime_error("Shared window is only available for OpenGL platform");
	}
	return m_SharedWindowHandle;
}


double AppWindowGLFW::_getTime() const
{
	return glfwGetTime();
}


void AppWindowGLFW::_setEventCallback()
{
	glfwSetCursorPosCallback(m_WindowHandle, [](GLFWwindow* window, double x, double y)
		{
			Timer timer("cursor event", true);
			MouseMoveEvent cursorMoveEvent(x, y);
			EventManager::getInstance().publish(cursorMoveEvent);
		});

	glfwSetScrollCallback(m_WindowHandle, [](GLFWwindow* window, double x, double y)
		{
			Timer timer("scroll event", false);
			MouseScrollEvent scrollEvent(x, y);
			EventManager::getInstance().publish(scrollEvent);
			//EventManager::getInstance().publish("mouseScrollEvent", x, y);
		});

	glfwSetKeyCallback(m_WindowHandle, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			//Application* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
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
				case GLFW_RELEASE: {
					//Console::println("released");
				}
				case GLFW_REPEAT: {
					//Console::println("key repeat");
				}
			}
		});

	glfwSetWindowSizeCallback(m_WindowHandle, [](GLFWwindow* window, int width, int height)
		{
			WindowResizeEvent resizeEvent(width, height);
			EventManager::getInstance().publish(resizeEvent);
		});

	glfwSetWindowCloseCallback(m_WindowHandle, [](GLFWwindow* window)
		{
			WindowCloseEvent closeEvent;
			EventManager::getInstance().publish(closeEvent);
		});

	//glfwSetFramebufferSizeCallback(m_WindowHandle, [](GLFWwindow* window)
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
	width = mode->width * 2 / 3;
	height = mode->height * 2 / 3;

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);	// Set Invisible for the second context
	m_SharedWindowHandle = glfwCreateWindow(width, height, "", NULL, m_WindowHandle);

	if (m_SharedWindowHandle == NULL) {
		throw std::runtime_error("Failed to create shared GLFW window");
		glfwTerminate();
		return -1;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	//glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);	// disable title bar

	m_WindowHandle = glfwCreateWindow(width, height, config.title.c_str(), NULL, NULL);

	if (m_WindowHandle == NULL) {
		throw std::runtime_error("Failed to create GLFW window");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(m_WindowHandle);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		throw std::runtime_error("Failed to initialize GLAD for main context");
		glfwDestroyWindow(m_WindowHandle);
		glfwTerminate();
		return -1;
	}
}

void AppWindowGLFW::_onCloseOpenGL()
{
	glfwMakeContextCurrent(nullptr);
	glfwDestroyWindow(m_WindowHandle);
	glfwDestroyWindow(m_SharedWindowHandle);
	glfwTerminate();
}

void AppWindowGLFW::_onUpdateOpenGL()
{
	glfwPollEvents();
	glfwSwapBuffers(m_WindowHandle);
	
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

	m_WindowHandle = glfwCreateWindow(width, height, config.title.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(m_WindowHandle, this);

	return 0;
}

void AppWindowGLFW::_onCloseVulkan()
{
	glfwDestroyWindow(m_WindowHandle);
	glfwTerminate();
}

void AppWindowGLFW::_onUpdateVulkan()
{
	glfwPollEvents();

	//throw std::runtime_error("Vulkan onUpdate not yet implemented");
}
