#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

#include "../../src/gui/GuiController.h"
#include "../../src/window/AppWindow.h"
#include "../../graphics/renderer/Renderer.h"
#include "configs.h"
#include "ServiceLocator.h"


// one single file to create all subsystems
// might separate into files per subsystem but good enough for now
class PlatformFactory
{
private:
	ServiceLocator& serviceLocator;

public:
	PlatformFactory(ServiceLocator& serviceLocator);
	~PlatformFactory() = default;

	std::unique_ptr<AppWindow> createWindow(WindowConfig config);
	std::unique_ptr<GuiManager> createGuiManager(GuiPlatform platform);
	std::unique_ptr<Renderer> createRenderer(RenderPlatform platform);
	//receive different configs from ui, window, graphics api, sound
	//physics and so on to create the appropriate system from the given config

private:
	typedef std::function<std::unique_ptr<AppWindow>(WindowConfig)> WindowConstructor;
	typedef std::function<std::unique_ptr<GuiManager>()> GuiConstructor;
	typedef std::function<std::unique_ptr<Renderer>()> RendererConstructor;
	//typedef std::function <std::unique_ptr<GPUDevice>() DeviceConstructors;
	
	std::unordered_map<WindowPlatform, WindowConstructor>  windowConstructors;
	std::unordered_map<GuiPlatform, GuiConstructor>  guiConstructors;
	std::unordered_map<RenderPlatform, RendererConstructor>  rendererConstructors;

};

