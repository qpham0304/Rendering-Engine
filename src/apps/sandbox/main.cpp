#include "../vulkan-demo/VulkanApplication.h"
#include "../vulkan-demo/example_glfw_vulkan.h"
#include "../../core/Application.h"
#include "../../src/apps/deferred-IBL-demo/deferredIBL_demo.h"
#include "../../src/apps/particle-demo/particleDemo.h"

//#define RUN_EDITOR
int main()
{
	try {
		WindowConfig windowConfig{};
#ifdef RUN_EDITOR
		windowConfig.title = "Engine Editor";
		windowConfig.windowPlatform = WindowPlatform::GLFW;
		windowConfig.renderPlatform = RenderPlatform::OPENGL;
		windowConfig.guiPlatform = GuiPlatform::IMGUI;
		windowConfig.os = OperatingSystem::WINDOW;
		windowConfig.width = 1920;
		windowConfig.height = 1080;
		windowConfig.vsync = false;

		Application app(windowConfig);
		app.init();

		app.pushLayer(new DeferredIBLDemo("demo"));
		//app.pushLayer(new ParticleDemo("particle demo"));

		app.start();

		app.run();
		app.end();
#else
		windowConfig.title = "Vulkan Application";
		windowConfig.windowPlatform = WindowPlatform::GLFW;
		windowConfig.renderPlatform = RenderPlatform::VULKAN;
		windowConfig.guiPlatform = GuiPlatform::IMGUI;
		windowConfig.os = OperatingSystem::WINDOW;
		windowConfig.width = 1920;
		windowConfig.height = 1080;
		windowConfig.vsync = false;

		VulkanApplication app;
		app.init(windowConfig);
		app.start();
		app.run();
		app.end();
#endif
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Exception caught by main: " << e.what() << std::endl;
	}
}