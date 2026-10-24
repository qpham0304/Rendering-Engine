#include <core/Engine.h>
#include "SandboxGame.h"

int main()
{
    try {
        WindowConfig windowConfig{};
        windowConfig.title = "Vulkan Sandbox";
        windowConfig.windowPlatform = WindowPlatform::SDL;
        windowConfig.renderPlatform = RenderPlatform::VULKAN;
        windowConfig.guiPlatform = GuiPlatform::IMGUI;
        windowConfig.os = OperatingSystem::WINDOW;
        windowConfig.scriptingPlatform = ScriptingPlatform::LUA;
	    windowConfig.physicsFramework = PhysicsFramework::BOX3D;
        windowConfig.width = 1920;
        windowConfig.height = 1080;
        windowConfig.vsync = false;
        windowConfig.targetRenderFPS = 144.0f;
        windowConfig.targetUpdateFPS = 144.0f;
#ifdef WK_WORKING_DIR
        windowConfig.WorkDir = WK_WORKING_DIR;  // dev build: share assets from the project source dir
#endif

        Engine app(windowConfig);
        wkGameLoad(app);
        app.init();
        app.start();
        app.run();
        app.close();
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Exception caught by main: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Caught standard exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught by main" << std::endl;
    }
}
