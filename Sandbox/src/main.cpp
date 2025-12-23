#include "Sandbox.h"
#include <core/Engine.h>
#include "SandboxLayer.h"

int main()
{
    try {
        WindowConfig windowConfig{};
        windowConfig.title = "Vulkan Sandbox";
        windowConfig.windowPlatform = WindowPlatform::GLFW;
        windowConfig.renderPlatform = RenderPlatform::VULKAN;
        windowConfig.guiPlatform = GuiPlatform::IMGUI;
        windowConfig.os = OperatingSystem::WINDOW;
        windowConfig.width = 1920;
        windowConfig.height = 1080;
        windowConfig.vsync = false;

        Sandbox app(windowConfig);
        app.pushLayer(new SandBoxLayer("Sandbox Layer"));
        app.init();
        app.start();
        app.run();
        app.close();
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Exception caught by main: " << e.what() << std::endl;
    } 
    catch (...) {
        std::cerr << "Unknown exception caught by main" << std::endl;
    }
}
