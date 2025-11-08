#include "PlatformFactory.h"
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"
#include "../../src/gui/framework/ImGui/ImGuiController.h"
#include "../../src/logging/framework/LoggerSpd.h"

#define REGISTER_WINDOW_CONSTRUCTOR(constructors_map, platformEnum, ClassType) \
    constructors_map[platformEnum] = [this]() { \
        auto appWindow = std::make_unique<ClassType>(); \
        this->serviceLocator.Register("AppWindow", *appWindow.get());   \
        return appWindow; \
    };

#define REGISTER_GUI_CONSTRUCTOR(constructors_map, platformEnum, ClassType) \
    constructors_map[platformEnum] = [this]() { \
        auto guiManager = std::make_unique<ClassType>();   \
        this->serviceLocator.Register("GuiManager", *guiManager.get());   \
        return guiManager;  \
    };

#define REGISTER_LOGGER_CONSTRUCTOR(constructors_map, platformEnum, ClassType) \
    constructors_map[platformEnum] = [this](std::string_view name) { \
        auto logger = std::make_unique<ClassType>(name.data());   \
        this->serviceLocator.Register(std::string("Logger.") + name.data(), *logger.get());   \
        return logger;  \
    };

PlatformFactory::PlatformFactory(ServiceLocator& serviceLocator)
    : serviceLocator(serviceLocator)
{
    REGISTER_WINDOW_CONSTRUCTOR(windowConstructors, WindowPlatform::GLFW, AppWindowGLFW);

    REGISTER_GUI_CONSTRUCTOR(guiConstructors, GuiPlatform::IMGUI, ImGuiController);

	REGISTER_LOGGER_CONSTRUCTOR(loggerConstructors, LoggerPlatform::SPDLOG, LoggerSpd);
	//loggerRegistry.Register<LoggerSpd>(LoggerPlatform::SPDLOG, serviceLocator, "Logger");
}

std::unique_ptr<AppWindow> PlatformFactory::createWindow(WindowPlatform platform)
{
    if (windowConstructors.find(platform) == windowConstructors.end()) {
        throw std::runtime_error("failed to create window, platform not supported");
    }
    return windowConstructors[platform]();
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

std::unique_ptr<Logger> PlatformFactory::createLogger(LoggerPlatform platform, std::string_view name)
{
    if (loggerConstructors.find(platform) == loggerConstructors.end()) {
        throw std::runtime_error("failed to create window, platform not supported");
    }
    return loggerConstructors[platform](name);
    //return loggerRegistry.Create(platform);
}
