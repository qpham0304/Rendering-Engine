#pragma once

#include <unordered_map>
#include <functional>
#include <memory>

#include "configs.h"
#include "ServiceLocator.h"
#include "gui/GuiManager.h"
#include "window/AppWindow.h"
#include "services/Service.h"
#include "graphics/renderers/RenderDevice.h"
#include "core/resources/managers/Manager.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/BufferManager.h"
#include "core/resources/managers/DescriptorManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "core/resources/managers/RendererManager.h"

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

	template<typename Interface, typename PlatformEnum, typename... Args>
	std::unique_ptr<Interface> Create(PlatformEnum platform, Args&&... args) {
		return GetFactory<Interface, PlatformEnum, Args...>().Create(platform, std::forward<Args>(args)...);
	}

	std::unique_ptr<Logger> Create(LoggerPlatform platform, std::string name);


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

	template<typename Interface, typename PlatformEnum, typename... Args>
	auto& GetFactory() {
		return std::get<Factory<Interface, PlatformEnum, Args...>>(factories);
	}


private:
	ServiceLocator& serviceLocator;
	
	std::tuple<
		Factory<AppWindow, WindowPlatform>,
		Factory<GuiManager, GuiPlatform>,
		Factory<RenderDevice, RenderPlatform>,
		Factory<TextureManager, RenderPlatform>,
		Factory<BufferManager, RenderPlatform>,
		Factory<DescriptorManager, RenderPlatform>,
		Factory<MaterialManager, RenderPlatform>,
		Factory<RendererManager, RenderPlatform>
	> factories;
};