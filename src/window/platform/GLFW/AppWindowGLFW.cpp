#include "AppWindowGLFW.h"
#include <stdexcept>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/eventManager.h"
#include "../../src/window/Input.h"

AppWindowGLFW::AppWindowGLFW() : AppWindow()
{

}

AppWindowGLFW::~AppWindowGLFW()
{

}

GLFWwindow* AppWindowGLFW::getWindow()
{
	return window;
}

GLFWwindow* AppWindowGLFW::getSharedWindow()
{
	return sharedWindow;
}

int AppWindowGLFW::init(WindowConfig newConfig) {
	config = newConfig;
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

	return 0;
}

int AppWindowGLFW::start() {
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

	glfwSwapInterval(1);
	//glfwSwapInterval(0);

	return 0;
}

int AppWindowGLFW::end() {
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



void AppWindowGLFW::setEventCallback()
{
	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y)
		{
			Timer timer("cursor event", true);
			MouseMoveEvent cursorMoveEvent(window, x, y);
			EventManager::getInstance().Publish(cursorMoveEvent);
		});

	glfwSetScrollCallback(window, [](GLFWwindow* window, double x, double y)
		{
			Timer timer("scroll event", true);
			MouseScrollEvent scrollEvent(window, x, y);
			EventManager::getInstance().Publish(scrollEvent);
			//EventManager::getInstance().Publish("mouseScrollEvent", x, y);
		});

	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			//Application* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
			switch (action) {
			case GLFW_PRESS: {
				if (key == KEY_ESCAPE) {
					WindowCloseEvent windowCloseEvent;
					EventManager::getInstance().Publish(windowCloseEvent);
				}
				else {
					KeyPressedEvent keyPressEvent(key);
					EventManager::getInstance().Publish(keyPressEvent);
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
			WindowResizeEvent resizeEvent(window, width, height);
			EventManager::getInstance().Publish(resizeEvent);
		});

	glfwSetWindowCloseCallback(window, [](GLFWwindow* window)
		{
			WindowCloseEvent closeEvent;
			EventManager::getInstance().Publish(closeEvent);
		});
}