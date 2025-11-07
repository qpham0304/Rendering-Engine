#include "AppWindowGLFW.h"
#include <stdexcept>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/eventManager.h"
#include "InputGLFW.h"

AppWindowGLFW::AppWindowGLFW() : AppWindow()
{
	input = std::make_unique<InputGLFW>();
}

AppWindowGLFW::~AppWindowGLFW()
{
	//input.reset();
}

void* AppWindowGLFW::getWindow()
{
	return window;
}

void* AppWindowGLFW::getSharedWindow()
{
	return sharedWindow;
}

int AppWindowGLFW::init(WindowConfig newConfig) {
	this->config = newConfig;
	width = config.width;
	height = config.height;

	AppWindow::platform = config.renderPlatform;
	if (supportRenderPlatform.find(config.renderPlatform) == supportRenderPlatform.end()) {
		throw std::runtime_error("Platform unsupported");
	}

	if (AppWindow::platform == RenderPlatform::OPENGL) {
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
	}

	if (platform == RenderPlatform::UNDEFINED) {
		throw std::runtime_error("Platform not specified have you called init() with supported platform yet?");
	}

	else if (platform == RenderPlatform::OPENGL) {
		window = glfwCreateWindow(width, height, config.title.c_str(), NULL, NULL);

		if (window == NULL) {
			throw std::runtime_error("Failed to create GLFW window");
			glfwTerminate();
			return -1;
		}
		glfwMakeContextCurrent(window);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			throw std::runtime_error("Failed to initialize GLAD for main context");
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}

		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);	// Set Invisible for second context
		sharedWindow = glfwCreateWindow(width, height, "", NULL, window);

		if (sharedWindow == NULL) {
			throw std::runtime_error("Failed to create shared GLFW window");
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}
		glfwMakeContextCurrent(sharedWindow);

		glfwMakeContextCurrent(window);
	}

	setEventCallback();

	//((InputGLFW*)input.get())->window = window;	// shorter but how can read
	InputGLFW* tmp = dynamic_cast<InputGLFW*>(input.get());
	if (!tmp) {
		throw std::runtime_error("AppWindowGLFW: failed to cast Input to type InputGLFW");
	}
	tmp->window = window;

	glfwSwapInterval(config.vsync);
	//glfwSwapInterval(1);
	//glfwSwapInterval(0);

	return 0;
}


int AppWindowGLFW::onClose() {
	if (platform == RenderPlatform::OPENGL) {
		glfwMakeContextCurrent(nullptr);
		glfwDestroyWindow(window);
		glfwDestroyWindow(sharedWindow);
		glfwTerminate();
	}
	return 0;
}

void AppWindowGLFW::onUpdate()
{
	if (AppWindow::platform == RenderPlatform::OPENGL) {
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
}

double AppWindowGLFW::getTime() const
{
	return glfwGetTime();
}



void AppWindowGLFW::setEventCallback()
{
	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y)
		{
			Timer timer("cursor event", true);
			MouseMoveEvent cursorMoveEvent(x, y);
			EventManager::getInstance().publish(cursorMoveEvent);
		});

	glfwSetScrollCallback(window, [](GLFWwindow* window, double x, double y)
		{
			Timer timer("scroll event", true);
			MouseScrollEvent scrollEvent(x, y);
			EventManager::getInstance().publish(scrollEvent);
			//EventManager::getInstance().publish("mouseScrollEvent", x, y);
		});

	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
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

	glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height)
		{
			WindowResizeEvent resizeEvent(width, height);
			EventManager::getInstance().publish(resizeEvent);
		});

	glfwSetWindowCloseCallback(window, [](GLFWwindow* window)
		{
			WindowCloseEvent closeEvent;
			EventManager::getInstance().publish(closeEvent);
		});
}