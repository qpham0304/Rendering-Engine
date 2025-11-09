#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

#include "../../src/gui/GuiManager.h"
#include "../../src/window/AppWindow.h"
#include "../../graphics/renderer/Renderer.h"
#include "../../logging/Logger.h"
#include "configs.h"
#include "ServiceLocator.h"

// one single file to create all subsystems
// might separate into files per subsystem but good enough for now
class PlatformFactory
{
public:
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
	std::unique_ptr<Logger> createLogger(LoggerPlatform platform, std::string_view name);
	//receive different configs from ui, window, graphics api, sound
	//physics and so on to create the appropriate system from the given config

private:
	ServiceLocator& serviceLocator;

	ConstructorRegistry<AppWindow, WindowPlatform> windowRegistry;
	ConstructorRegistry<GuiManager, GuiPlatform> guiRegistry;
	ConstructorRegistry<Renderer, RenderPlatform> rendererRegistry;
	ConstructorRegistry<Logger, LoggerPlatform, std::string_view> loggerRegistry;
};

