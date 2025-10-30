#include "AppWindow.h"
#include "Timer.h"
#include "../../events/Event.h"
#include "../../events/EventManager.h"
#include <camera.h>
#include "../../core/scene/SceneManager.h"
//#include "../../gui/"

unsigned int AppWindow::width = DEFAULT_WIDTH;
unsigned int AppWindow::height = DEFAULT_HEIGHT;
bool AppWindow::VsyncEnabled = false;

Platform AppWindow::platform = PLATFORM_UNDEFINED;
const std::set<Platform> AppWindow::supportPlatform = { PLATFORM_OPENGL };	// add more platform when we support more

GLFWwindow* AppWindow::window = nullptr;
GLFWwindow* AppWindow::sharedWindow = nullptr;



int AppWindow::init(Platform platform) {
	AppWindow::platform = platform;
	if (supportPlatform.find(platform)  == supportPlatform.end()) {
		throw std::runtime_error("Platform unsupported");
	}

	if (AppWindow::platform == PLATFORM_OPENGL) {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		// Initialize GLFW
		if (!glfwInit())
			return -1;

		// Get primary monitor
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		if (!monitor) {
			std::cerr << "Failed to get primary monitor" << std::endl;
			glfwTerminate();
			return -1;
		}

		// Get monitor video mode
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		if (!mode) {
			std::cerr << "Failed to get video mode" << std::endl;
			glfwTerminate();
			return -1;
		}

		width = mode->width * 2 / 3;
		height = mode->height * 2 / 3;
	}

	return 0;
}

int AppWindow::start(const char* title) {
	if (platform == PLATFORM_UNDEFINED) {
		throw std::runtime_error("Platform not specified have you called init() with supported platform yet?");
	}

	else if (platform == PLATFORM_OPENGL) {
		window = glfwCreateWindow(width, height, title, NULL, NULL);

		if (window == NULL) {
			std::cerr << "Failed to create GLFW window" << std::endl;
			glfwTerminate();
			return -1;
		}
		glfwMakeContextCurrent(window);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			std::cerr << "Failed to initialize GLAD for main context" << std::endl;
			glfwDestroyWindow(window);
			glfwTerminate();
			return -1;
		}

		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);	// Set Invisible for second context
		sharedWindow = glfwCreateWindow(width, height, "", NULL, window);

		if (sharedWindow == NULL) {
			std::cerr << "Failed to create shared GLFW window" << std::endl;
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

int AppWindow::end() {
	if (platform == PLATFORM_OPENGL) {
		glfwMakeContextCurrent(nullptr);
		glfwDestroyWindow(window);
		glfwDestroyWindow(sharedWindow);
		glfwTerminate();
	}
	return 0;
}

void AppWindow::setEventCallback()
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

void AppWindow::onUpdate()
{
	if (AppWindow::platform == PLATFORM_OPENGL) {
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
}