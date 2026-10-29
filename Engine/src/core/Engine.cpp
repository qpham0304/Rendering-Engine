#include "Engine.h"
#include <ranges>
#include <chrono>
#include <thread>
#include <cmath>

#include <window/AppWindow.h>
#include <core/events/EventManager.h>
#include <core/features/EngineStates.h>
#include <core/layers/EditorLayer.h>
#include <core/scene/SceneManager.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/BufferManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/SkeletonManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/resources/managers/MaterialManager.h>
#include <graphics/renderers/Renderer.h>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static std::filesystem::path getExecutableDir()
{
#ifdef _WIN32
	wchar_t buffer[MAX_PATH];
	DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	return std::filesystem::path(std::wstring(buffer, length)).parent_path();
#else
	return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

// must run before any service touches the file system
static void setupWorkingDirectory(WindowConfig& config)
{
	std::filesystem::path workDir = config.WorkDir.empty() ? getExecutableDir() : std::filesystem::path(config.WorkDir);
	std::error_code ec;
	std::filesystem::current_path(workDir, ec);
	if (ec) {
		std::cerr << "Engine: failed to set working directory to " << workDir << ": " << ec.message() << std::endl;
		return;
	}
	config.WorkDir = std::filesystem::current_path().string();
	if (!std::filesystem::exists(config.AssetsDir)) {
		std::cerr << "Engine: assets directory not found: " << (workDir / config.AssetsDir) << std::endl;
	}
}

Engine::Engine(WindowConfig config)
	: windowConfig(config),
	sceneManager(SceneManager::getInstance()),
	eventManager(EventManager::getInstance())
{
	setupWorkingDirectory(windowConfig);
	setFootprint("Engine", sizeof(Engine), MemorySystem::Core);
	ServiceLocator::setContext(&serviceLocator);
	engineLogger = platformFactory.Create(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.Create(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.Create<AppWindow>(windowConfig.windowPlatform);
	renderDevice = platformFactory.Create<RenderDevice>(windowConfig.renderPlatform);
	bufferManager = platformFactory.Create<BufferManager>(windowConfig.renderPlatform);
	descriptorManager = platformFactory.Create<DescriptorManager>(windowConfig.renderPlatform);
	textureManager = platformFactory.Create<TextureManager>(windowConfig.renderPlatform);
	materialManager = platformFactory.Create<MaterialManager>(windowConfig.renderPlatform);
	guiManager = platformFactory.Create<GuiManager>(windowConfig.guiPlatform);
	rendererManager = platformFactory.Create<RendererManager>(windowConfig.renderPlatform);
	scriptManager = platformFactory.Create<ScriptManager>(windowConfig.scriptingPlatform);
	physicsManager = platformFactory.Create<PhysicsManager>(windowConfig.physicsFramework);
	meshManager = platformFactory.Create<MeshManager>();
	skeletonManager = platformFactory.Create<SkeletonManager>();
	modelManager = platformFactory.Create<ModelManager>();
	layerManager = platformFactory.Create<LayerManager>();
	animationManager = platformFactory.Create<AnimationManager>();
	particleManager = platformFactory.Create<ParticleManager>();

	//NOTE: setup order is important!
	services.push_back(&eventManager);
	services.push_back(&sceneManager);
	services.push_back(appWindow.get());
	services.push_back(renderDevice.get());
	services.push_back(bufferManager.get());
	services.push_back(descriptorManager.get());
	services.push_back(textureManager.get());
	services.push_back(materialManager.get());
	services.push_back(meshManager.get());
	services.push_back(skeletonManager.get());
	services.push_back(modelManager.get());
	services.push_back(guiManager.get());
	services.push_back(animationManager.get());
	services.push_back(particleManager.get());
	services.push_back(scriptManager.get());
	services.push_back(physicsManager.get());
	services.push_back(layerManager.get());
	services.push_back(rendererManager.get());
}

void Engine::pushLayer(Layer* layer)
{
	layerManager->addLayer(layer);
	
	// layers pushed after init will miss LayerManager::init call
	if (isInitialized && !layer->init()) {
		engineLogger->error("Layer Initilize failed: {}", layer->getName());
	}
}

void Engine::removeLayer(Layer* layer)
{
	layerManager->removeLayer(layer);
}

void Engine::addFrameCallback(std::function<void()> callback)
{
	frameCallbacks.push_back(std::move(callback));
}

void Engine::init()
{
// #ifdef GAME_RELEASE
	pushLayer(new EditorLayer("EditorLayer", *guiManager));
	engineLogger->setLevel(LogLevel::Debug);
// #endif
	
	try {
		for (Service*& service : services) {
			if (!service->init(windowConfig)) {
				engineLogger->error("Service Initilize failed: {}", service->getServiceName());
			}
			else {
				engineLogger->debug("Initilize Service: {}", service->getServiceName());
			}
			initializedServices++;
		}
	}
	catch (const std::exception& e) {
		engineLogger->critical("Engine init failed in {}: {}", services[initializedServices]->getServiceName(), e.what());
		close();
		throw;
	}
	isInitialized = true;
}

void Engine::start()
{
	m_eventSubscriptions.push_back(eventManager.subscribe(EventType::WindowClose, [this](Event& event) {
		isRunning = false;
	}));

	m_eventSubscriptions.push_back(eventManager.subscribe(EventType::KeyPressed, [this](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_ESCAPE) {
			isRunning = false;
		}
		
		else if (keyPressedEvent.keyCode == KEY_F4) {
			isFullScreen = !isFullScreen;
			AppWindow::setFullscreen(isFullScreen, true);
		}
	}));
}

void Engine::run() {
	try {
		_runLoop();
	}
	catch (const std::exception& e) {
		engineLogger->critical("Engine stopped by an unhandled exception: {}", e.what());
		close();
		throw;
	}
}

void Engine::_runLoop() {
	const int maxFixedSteps = 5;
	const double maxFrameTime = 0.25;
	const double fixedDeltaTime = 1.0 / windowConfig.targetUpdateFPS;

	// limit frame when vsync is off and target render isnt zero, if vsync on ignore targetfps
	const bool limitFrameRate = !windowConfig.vsync && windowConfig.targetRenderFPS > 0.0f;
	const double targetFrameTime = limitFrameRate ? 1.0 / windowConfig.targetRenderFPS : 0.0;

	double accumulator = 0.0;
	double lastTime = AppWindow::getTime();
	bool wasPlaying = false;
	double nextFrameStart = lastTime;

	while (isRunning) {
		double currentTime = AppWindow::getTime();
		double deltaTime = std::min(currentTime - lastTime, maxFrameTime);
		lastTime = currentTime;

		for (auto& callback : frameCallbacks) {
			callback();
		}

		bool isPlaying = EngineState::isPlaying();
		if (isPlaying && !wasPlaying) {
			accumulator = 0.0;	// starting or resuming should not replay the time spent stopped
		}
		wasPlaying = isPlaying;

		FrameTime frameTime;
		frameTime.time = currentTime;
		frameTime.deltaTime = static_cast<float>(deltaTime);
		frameTime.gameDeltaTime = isPlaying ? static_cast<float>(deltaTime) : 0.0f;
		frameTime.fixedDeltaTime = static_cast<float>(fixedDeltaTime);

		appWindow->onUpdate(frameTime);	// poll input first for current frame's fixed steps

		if (isPlaying) {
			accumulator += deltaTime;
			int fixedSteps = 0;
			while (accumulator >= fixedDeltaTime && fixedSteps < maxFixedSteps) {
				for (Service* service : services) {
					service->onFixedUpdate(frameTime.fixedDeltaTime);
				}
				accumulator -= fixedDeltaTime;
				fixedSteps++;
			}

			// hit the catch up cap, drop the backlog instead of spiraling further behind every frame
			if (accumulator >= fixedDeltaTime) {
				accumulator = std::fmod(accumulator, fixedDeltaTime);
			}
			frameTime.alpha = static_cast<float>(accumulator / fixedDeltaTime);
		}

		for (Service* service : services) {
			if (service == appWindow.get()) {
				continue;
			}
			service->onUpdate(frameTime);
		}

		rendererManager->render();

		if (limitFrameRate) {
			nextFrameStart += targetFrameTime;
			double now = AppWindow::getTime();
			if (nextFrameStart < now) {
				nextFrameStart = now;	// fell behind, don't rush the next frames to make it up
			}
			else {
				AppWindow::sleepUntil(nextFrameStart);
			}
		}
	}
}

void Engine::close()
{
	// only services whose init finished, in reverse, and only once
	while (initializedServices > 0) {
		Service* service = services[--initializedServices];
		if (!service->onClose()) {
			engineLogger->error("Service Close failed: {}", service->getServiceName());
		}
		else {
			engineLogger->debug("Closing Service: {}", service->getServiceName());
		}
	}
	isInitialized = false;
}
