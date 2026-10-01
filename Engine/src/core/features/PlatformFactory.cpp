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
#include "graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h"
#include "graphics/framework/Vulkan/renderers/RendererManagerVulkan.h"
#include "scripting/framework/ScriptManagerLua.h"
#include "physics/framework/PhysicsManagerBox3D.h"

PlatformFactory::PlatformFactory(ServiceLocator& serviceLocator)
    : serviceLocator(serviceLocator)
{
    GetFactory<AppWindow, WindowPlatform>().Register(
        WindowPlatform::GLFW,
        RegisterConstructor<AppWindow, AppWindowGLFW>()
    );

    GetFactory<GuiManager, GuiPlatform>().Register(
        GuiPlatform::IMGUI,
        RegisterConstructor<GuiManager, ImGuiManager>()
    );

    GetFactory<RenderDevice, RenderPlatform>().Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<RenderDevice, RenderDeviceVulkan>()
    );

    GetFactory<RenderDevice, RenderPlatform>().Register(
        RenderPlatform::OPENGL,
        RegisterConstructor<RenderDevice, RenderDeviceOpenGL>()
    );

    GetFactory<TextureManager, RenderPlatform>().Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<TextureManager, TextureManagerVulkan>()
    );

    GetFactory<BufferManager, RenderPlatform>().Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<BufferManager, BufferManagerVulkan>()
    );

    GetFactory<DescriptorManager, RenderPlatform>().Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<DescriptorManager, DescriptorManagerVulkan>()
    );

    GetFactory<MaterialManager, RenderPlatform>().Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<MaterialManager, MaterialManagerVulkan>()
    );

    GetFactory<RendererManager, RenderPlatform>().Register(
        RenderPlatform::VULKAN,
        RegisterConstructor<RendererManager, RendererManagerVulkan>()
    );

    GetFactory<ScriptManager, ScriptingPlatform>().Register(
        ScriptingPlatform::LUA,
        RegisterConstructor<ScriptManager, ScriptManagerLua>()
    );

    GetFactory<PhysicsManager, PhysicsFramework>().Register(
        PhysicsFramework::BOX3D,
        RegisterConstructor<PhysicsManager, PhysicsManagerBox3D>()
    );
    
}

std::unique_ptr<Logger> PlatformFactory::Create(LoggerPlatform platform, std::string name)
{
    std::unique_ptr<Logger> logger;

    if (platform == LoggerPlatform::SPDLOG) {
        logger = std::make_unique<LoggerSpd>(name);
    }
    else {
        throw std::runtime_error("platform not supported");
    }

    serviceLocator.Register(logger->name(), *logger.get());
    return logger;
}