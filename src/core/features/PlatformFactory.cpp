#include "PlatformFactory.h"
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"
#include "../../src/gui/framework/ImGui/ImGuiController.h"

#define REGISTER_WINDOW_CONSTRUCTOR(constructors_map, platformEnum, ClassType) \
    constructors_map[platformEnum] = [](WindowConfig config) { \
        auto window = std::make_unique<ClassType>(); \
        window->init(config); \
        return window; \
    };

#define REGISTER_GUI_CONSTRUCTOR(constructors_map, platformEnum, ClassType) \
    constructors_map[platformEnum] = []() { \
        auto guiManager = std::make_unique<ClassType>();   \
        return guiManager;  \
    };

PlatformFactory::PlatformFactory()
{
    //windowConstructors[WindowPlatform::GLFW] = [](WindowConfig config) {
    //    std::unique_ptr<AppWindow> appWindow = std::make_unique<AppWindowGLFW>();
    //    appWindow->init(config.renderPlatform);
    //    return appWindow;
    //};
    REGISTER_WINDOW_CONSTRUCTOR(windowConstructors, WindowPlatform::GLFW, AppWindowGLFW)


    //guiConstructors[GuiPlatform::IMGUI] = []() {
    //    std::unique_ptr<GuiManager> guiManager = std::make_unique<ImGuiController>();
    //    return guiManager;
    //};
    REGISTER_GUI_CONSTRUCTOR(guiConstructors, GuiPlatform::IMGUI, ImGuiController)
}

PlatformFactory& PlatformFactory::getInstance()
{
    static PlatformFactory instance;
    return instance;
}

std::unique_ptr<AppWindow> PlatformFactory::createWindow(WindowConfig config)
{
    if (windowConstructors.find(config.windowPlatform) == windowConstructors.end()) {
        throw std::runtime_error("failed to create window, platform not supported");
    }
    return windowConstructors[config.windowPlatform](config);
}

std::unique_ptr<GuiManager> PlatformFactory::createGuiManager(GuiPlatform platform)
{
    if (guiConstructors.find(platform) == guiConstructors.end()) {
        throw std::runtime_error("failed to create window, platform not supported");
    }
    return guiConstructors[platform]();
}

std::unique_ptr<Renderer> PlatformFactory::createRenderer(RenderPlatform platform)
{
    if (rendererConstructors.find(platform) == rendererConstructors.end()) {
        throw std::runtime_error("failed to create window, platform not supported");
    }
    return rendererConstructors[platform]();
}
