#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

#include "configs.h"
#include "ServiceLocator.h"
#include "gui/GuiManager.h"
#include "window/AppWindow.h"
#include "graphics/renderers/Renderer.h"
#include "logging/Logger.h"
#include "services/Service.h"
#include "graphics/renderers/RenderDevice.h"
#include "core/resources/managers/Manager.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/BufferManager.h"
#include "core/resources/managers/DescriptorManager.h"
#include "core/resources/managers/MaterialManager.h"

// one single file to register and create all subsystems and services
// might separate into files per subsystem but good enough for now
class PlatformFactory
{
	// generic constructor function for each service interface
	template<typename Interface, typename PlatformEnum, typename... Args>
	class Factory {
	public:
		using Constructor = std::function<std::unique_ptr<Interface>(Args...)>;
		std::unordered_map<PlatformEnum, Constructor> constructors;

		void Register(PlatformEnum key, Constructor constructor) {
			constructors[key] = constructor;
		}

		template<typename... CallArgs>
		std::unique_ptr<Interface> Create(PlatformEnum platform, CallArgs&&... args) {
			auto it = constructors.find(platform);
			if (it == constructors.end()) {
				throw std::runtime_error("Given Platform has no registered constructor");
			}
			return it->second(std::forward<CallArgs>(args)...);
		}
	};


public:
	PlatformFactory(ServiceLocator& serviceLocator);
	~PlatformFactory() = default;

	template<typename Interface, typename Concrete, typename... Args>
	std::unique_ptr<Interface> create(WindowPlatform config, Args... args) {

	}

	std::unique_ptr<Logger> createLogger(LoggerPlatform platform, std::string_view name);
	std::unique_ptr<AppWindow> createWindow(WindowPlatform config);
	std::unique_ptr<GuiManager> createGuiManager(GuiPlatform config);
	std::unique_ptr<Renderer> createRenderer(RenderPlatform platform);
	std::unique_ptr<RenderDevice> createRenderDevice(RenderPlatform platform);
	std::unique_ptr<TextureManager> createTextureManager(RenderPlatform platform);
	std::unique_ptr<BufferManager> createBufferManager(RenderPlatform platform);
	std::unique_ptr<DescriptorManager> createDescriptorManager(RenderPlatform platform);
	std::unique_ptr<MaterialManager> createMaterialManager(RenderPlatform platform);


private:
	template<typename Interface, typename Concrete, typename... Args>
		requires std::derived_from<Interface, Service>
	auto RegisterConstructor(std::string_view customName = "") {
		return [this, customName](Args&&... args) -> std::unique_ptr<Interface> {
			std::unique_ptr<Interface> instance = std::make_unique<Concrete>(std::forward<Args>(args)...);
			std::string serviceName = customName.empty() ? instance->getServiceName() : customName.data();
			serviceLocator.Register<Interface>(serviceName, *instance);
			return instance;
		};
	}


private:
	ServiceLocator& serviceLocator;

	Factory<Logger, LoggerPlatform, std::string> loggerFactory;
	Factory<AppWindow, WindowPlatform> windowFactory;
	Factory<GuiManager, GuiPlatform> guiFactory;
	Factory<Renderer, RenderPlatform> rendererFactory;
	Factory<RenderDevice, RenderPlatform> renderDeviceFactory;
	Factory<TextureManager, RenderPlatform> textureManagerFactory;
	Factory<BufferManager, RenderPlatform> bufferManagerFactory;
	Factory<DescriptorManager, RenderPlatform> descriptorManagerFactory;
	Factory<MaterialManager, RenderPlatform> materialManagerFactory;
};