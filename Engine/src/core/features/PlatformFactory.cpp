#include "PlatformFactory.h"
#include "window/platform/GLFW/AppWindowGLFW.h"
#include "gui/framework/ImGui/ImGuiManager.h"
#include "logging/framework/LoggerSpd.h"
#include "graphics/framework/Vulkan/RenderDeviceVulkan.h"
#include "graphics/framework/OpenGL/RenderDeviceOpenGL.h"
#include "graphics/framework/Vulkan/renderers/RendererVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/buffers/VulkanBufferManager.h"

PlatformFactory::PlatformFactory(ServiceLocator& serviceLocator)
    : serviceLocator(serviceLocator)
{
    loggerRegistry.Register(
        LoggerPlatform::SPDLOG,
        RegisterConstructor<Logger, LoggerSpd, std::string>("Logger")
    );

    windowRegistry.Register(
        WindowPlatform::GLFW,
        RegisterConstructor<AppWindow, AppWindowGLFW>("AppWindow")
    );

    guiRegistry.Register(
        GuiPlatform::IMGUI,
        RegisterConstructor<GuiManager, ImGuiManager>("GuiManager")
    );

    rendererRegistry.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<Renderer, RendererVulkan>("RendererVulkan")
    );

    renderDeviceRegistry.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<RenderDevice, RenderDeviceVulkan>("RenderDeviceVulkan")
    );

    renderDeviceRegistry.Register(
        RenderPlatform::OPENGL,
        RegisterConstructor<RenderDevice, RenderDeviceOpenGL>("RenderDeviceOpenGL")
    );

    textureManagerRegistry.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<TextureManager, TextureManagerVulkan>("TextureManagerVulkan")
    );

     bufferManagerRegistry.Register(
         RenderPlatform::VULKAN,
         RegisterConstructor<BufferManager, VulkanBufferManager>("BufferManagerVulkan")
     );
    
}

std::unique_ptr<Logger> PlatformFactory::createLogger(LoggerPlatform platform, std::string_view name)
{
    return loggerRegistry.Create(platform, name.data());
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

std::unique_ptr<RenderDevice> PlatformFactory::createRenderDevice(RenderPlatform platform)
{
    return renderDeviceRegistry.Create(platform);
}

std::unique_ptr<TextureManager> PlatformFactory::createTextureManager(RenderPlatform platform)
{
    return textureManagerRegistry.Create(platform);
}

std::unique_ptr<BufferManager> PlatformFactory::createBufferManager(RenderPlatform platform)
{
    return bufferManagerRegistry.Create(platform);
}

