#include <core/Engine.h>
// #include "deferred-IBL-demo/deferredIBL_demo.h"
// #include "particle-demo/ParticleDemo.h"

int main()
{
	// try {
		WindowConfig windowConfig{};
		windowConfig.title = "Engine Editor";
		windowConfig.windowPlatform = WindowPlatform::GLFW;
		windowConfig.renderPlatform = RenderPlatform::OPENGL;
		windowConfig.guiPlatform = GuiPlatform::IMGUI;
		windowConfig.os = OperatingSystem::WINDOW;
		windowConfig.width = 1920;
		windowConfig.height = 1080;
		windowConfig.vsync = false;

		Engine app(windowConfig);
		app.init();

		// app.pushLayer(new DeferredIBLDemo("demo"));
		//app.pushLayer(new ParticleDemo("particle demo"));

		app.start();

		app.run();
		app.end();
	// }
	// catch (const std::runtime_error& e) {
	// 	std::cerr << "Exception caught by main: " << e.what() << std::endl;
	// }
}