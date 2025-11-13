#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

#include "../../src/gui/GuiManager.h"
#include "../../src/window/AppWindow.h"
#include "../../src/graphics/renderers/Renderer.h"
#include "../../src/logging/Logger.h"
#include "../../src/services/Service.h"
#include "../../src/graphics/RenderDevice.h"
#include "configs.h"
#include "ServiceLocator.h"

// one single file to registery and create all subsystems and services
// might separate into files per subsystem but good enough for now
class PlatformFactory
{
	// generic constructor function for each service interface
	template<typename Interface, typename PlatformEnum, typename... Args>
	class ConstructorRegistry {
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
				throw std::runtime_error("No constructor registered for the given platform.");
			}
			return it->second(std::forward<CallArgs>(args)...);
		}
	};


public:
	PlatformFactory(ServiceLocator& serviceLocator);
	~PlatformFactory() = default;

	std::unique_ptr<AppWindow> createWindow(WindowPlatform config);
	std::unique_ptr<GuiManager> createGuiManager(GuiPlatform config);
	std::unique_ptr<Renderer> createRenderer(RenderPlatform platform);
	std::unique_ptr<RenderDevice> createRenderDevice(RenderPlatform platform);
	std::unique_ptr<Logger> createLogger(LoggerPlatform platform, std::string_view name);


private:
	template<typename Interface, typename Concrete, typename... Args>
		requires std::derived_from<Interface, Service>
	auto RegisterConstructor(std::string_view serviceName) {
		return [this, serviceName](Args&&... args) -> std::unique_ptr<Interface> {
			std::unique_ptr<Interface> instance = std::make_unique<Concrete>(std::forward<Args>(args)...);
			serviceLocator.Register(instance->getServiceName(), *instance);
			return instance;
		};
	}


private:
	ServiceLocator& serviceLocator;

	ConstructorRegistry<AppWindow, WindowPlatform> windowRegistry;
	ConstructorRegistry<GuiManager, GuiPlatform> guiRegistry;
	ConstructorRegistry<Renderer, RenderPlatform> rendererRegistry;
	ConstructorRegistry<RenderDevice, RenderPlatform> renderDeviceRegistry;
	ConstructorRegistry<Logger, LoggerPlatform, std::string> loggerRegistry;
};