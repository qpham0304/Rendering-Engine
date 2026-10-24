#include <core/Engine.h>
#include "GameModule.h"

int main()
{
    try {
        WindowConfig windowConfig{};
        windowConfig.title = "Vulkan Editor";
        windowConfig.windowPlatform = WindowPlatform::GLFW;
        windowConfig.renderPlatform = RenderPlatform::VULKAN;
        windowConfig.guiPlatform = GuiPlatform::IMGUI;
        windowConfig.os = OperatingSystem::WINDOW;
        windowConfig.width = 1920;
        windowConfig.height = 1080;
        windowConfig.vsync = false;
#ifdef WK_WORKING_DIR
        windowConfig.WorkDir = WK_WORKING_DIR;      // game project dir, the editor works on its source assets
#endif

        Engine app(windowConfig);
        GameModule game(app, WK_GAME_MODULE_PATH);  //game dll layers need to be removed before engine destroy
        game.load();
        app.addFrameCallback([&game]() { game.reloadIfChanged(); });
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
