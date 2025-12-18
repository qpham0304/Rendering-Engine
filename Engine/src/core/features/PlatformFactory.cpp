#include "PlatformFactory.h"
#include "window/platform/GLFW/AppWindowGLFW.h"
#include "gui/framework/ImGui/ImGuiManager.h"
#include "logging/framework/LoggerSpd.h"
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/OpenGL/renderers/RenderDeviceOpenGL.h"
#include "graphics/framework/Vulkan/renderers/RendererVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h"

PlatformFactory::PlatformFactory(ServiceLocator& serviceLocator)
    : serviceLocator(serviceLocator)
{
    loggerFactory.Register(
        LoggerPlatform::SPDLOG,
        RegisterConstructor<Logger, LoggerSpd, std::string>()
    );

    windowFactory.Register(
        WindowPlatform::GLFW,
        RegisterConstructor<AppWindow, AppWindowGLFW>()
    );

    guiFactory.Register(
        GuiPlatform::IMGUI,
        RegisterConstructor<GuiManager, ImGuiManager>()
    );

    rendererFactory.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<Renderer, RendererVulkan>()
    );

    renderDeviceFactory.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<RenderDevice, RenderDeviceVulkan>()
    );

    renderDeviceFactory.Register(
        RenderPlatform::OPENGL,
        RegisterConstructor<RenderDevice, RenderDeviceOpenGL>()
    );

    textureManagerFactory.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<TextureManager, TextureManagerVulkan>()
    );

    bufferManagerFactory.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<BufferManager, BufferManagerVulkan>()
    );

    descriptorManagerFactory.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<DescriptorManager, DescriptorManagerVulkan>()
    );

    materialManagerFactory.Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<MaterialManager, MaterialManagerVulkan>()
    );
    
}

std::unique_ptr<Logger> PlatformFactory::createLogger(LoggerPlatform platform, std::string_view name)
{
    return loggerFactory.Create(platform, name.data());
}

std::unique_ptr<AppWindow> PlatformFactory::createWindow(WindowPlatform platform)
{
	return windowFactory.Create(platform);
}

std::unique_ptr<GuiManager> PlatformFactory::createGuiManager(GuiPlatform platform)
{
	return guiFactory.Create(platform);
}
std::unique_ptr<Renderer> PlatformFactory::createRenderer(RenderPlatform platform)
{
	return rendererFactory.Create(platform);
}

std::unique_ptr<RenderDevice> PlatformFactory::createRenderDevice(RenderPlatform platform)
{
    return renderDeviceFactory.Create(platform);
}

std::unique_ptr<TextureManager> PlatformFactory::createTextureManager(RenderPlatform platform)
{
    return textureManagerFactory.Create(platform);
}

std::unique_ptr<BufferManager> PlatformFactory::createBufferManager(RenderPlatform platform)
{
    return bufferManagerFactory.Create(platform);
}

std::unique_ptr<DescriptorManager> PlatformFactory::createDescriptorManager(RenderPlatform platform)
{
    return descriptorManagerFactory.Create(platform);
}

std::unique_ptr<MaterialManager> PlatformFactory::createMaterialManager(RenderPlatform platform)
{
    return materialManagerFactory.Create(platform);
}

