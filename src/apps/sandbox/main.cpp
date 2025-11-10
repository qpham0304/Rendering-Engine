
#include "../vulkan-demo/VulkanSetupDemo.h"
#include "../../core/Application.h"
#include "../../src/apps/deferred-IBL-demo/deferredIBL_demo.h"
#include "../../src/apps/particle-demo/particleDemo.h"

//#define RUN_STANDALONE
int main()
{
	#ifdef RUN_STANDALONE

	WindowConfig windowConfig{};
	windowConfig.title = "Application Untitled";
	windowConfig.windowPlatform = WindowPlatform::GLFW;
	windowConfig.renderPlatform = RenderPlatform::OPENGL;
	windowConfig.guiPlatform = GuiPlatform::IMGUI;
	windowConfig.width = 1920;
	windowConfig.height = 1080;
	windowConfig.vsync = false;

	Application app(windowConfig);
	app.init();

	try {
		app.start();

		//TODO: problem: app layer can only be added after
		// everything is started so check that before allow adding layer
		app.pushLayer(new DeferredIBLDemo("demo"));
		//app.pushLayer(new ParticleDemo("particle demo"));

		app.run();
		app.end();
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Exception caught by main: " << e.what() << std::endl;
	}

	#else
	try {
		VulkanSetupDemo::run();
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Exception caught by main: " << e.what() << std::endl;
	}
	#endif
}