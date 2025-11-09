#include "PlatformFactory.h"
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"
#include "../../src/gui/framework/ImGui/ImGuiManager.h"
#include "../../src/logging/framework/LoggerSpd.h"

PlatformFactory::PlatformFactory(ServiceLocator& serviceLocator)
    : serviceLocator(serviceLocator)
{
    windowRegistry.Register(WindowPlatform::GLFW,
        [this]() -> std::unique_ptr<AppWindow> {
            auto appWindow = std::make_unique<AppWindowGLFW>();
            this->serviceLocator.Register("AppWindow", *appWindow);
            return appWindow;
        }
	);

    guiRegistry.Register(GuiPlatform::IMGUI,
        [this]() -> std::unique_ptr<GuiManager> {
            auto guiManager = std::make_unique<ImGuiManager>();
            this->serviceLocator.Register("GuiManager", *guiManager);
            return guiManager;
        }
	);
    
    loggerRegistry.Register(LoggerPlatform::SPDLOG,
        [this](std::string_view name) -> std::unique_ptr<Logger> {
            auto logger = std::make_unique<LoggerSpd>(name.data());
            this->serviceLocator.Register(std::string("Logger") + std::string(name), *logger);
            return logger;
        }
    );
}

std::unique_ptr<AppWindow> PlatformFactory::createWindow(WindowPlatform platform)
{
	return windowRegistry.Create(platform);
}

std::unique_ptr<GuiManager> PlatformFactory::createGuiManager(GuiPlatform platform)
{
	return guiRegistry.Create(platform);
}

std::unique_ptr<Renderer> PlatformFactory::createRenderer(RenderPlatform platform)
{
	return rendererRegistry.Create(platform);
}

std::unique_ptr<Logger> PlatformFactory::createLogger(LoggerPlatform platform, std::string_view name)
{
    return loggerRegistry.Create(platform, name);
}
