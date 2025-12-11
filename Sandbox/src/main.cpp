#include "VulkanApplication.h"
#include <core/Engine.h>


int main()
{
    try {
        WindowConfig windowConfig{};
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
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Exception caught by main: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught by main" << std::endl;
    }
}
