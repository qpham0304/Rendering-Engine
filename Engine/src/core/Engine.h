#pragma once

#include "Engine/EngineCoreAPI.h"
#include <core/features/ServiceLocator.h>
#include <core/features/PlatformFactory.h>
#include <services/Service.h>
#include <core/layers/LayerManager.h>
#include <core/scene/SceneManager.h>
#include <animation/AnimationManager.h>
#include <particle/ParticleManager.h>
#include <core/resources/managers/SkeletonManager.h>
#include "core/features/Object.h"
#include <functional>

class Layer;
class EventManager;

class MYGRAPHICSENGINE_EXPORT Engine : public Object
{
public:
    Engine(WindowConfig config);
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void pushLayer(Layer* layer);
    void removeLayer(Layer* layer);
    void addFrameCallback(std::function<void()> callback); // start of every frame, outside dll layers to add/remove
    void init();
    void start();
    void run();
    void close();

private:
    bool showGui = false;
    bool isRunning = true;
    bool isFullScreen = false;
    WindowConfig windowConfig;
    ServiceLocator serviceLocator;
    PlatformFactory platformFactory{ serviceLocator };

    SceneManager& sceneManager;
    EventManager& eventManager;
    std::unique_ptr<AppWindow> appWindow;
    std::unique_ptr<LayerManager> layerManager;
    std::unique_ptr<GuiManager> guiManager;
    std::unique_ptr<RenderDevice> renderDevice;
    std::unique_ptr<Logger> engineLogger;
    std::unique_ptr<Logger> clientLogger;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<BufferManager> bufferManager;
    std::unique_ptr<MeshManager> meshManager;
    std::unique_ptr<SkeletonManager> skeletonManager;
    std::unique_ptr<ModelManager> modelManager;
    std::unique_ptr<DescriptorManager> descriptorManager;
    std::unique_ptr<MaterialManager> materialManager;
    std::unique_ptr<RendererManager> rendererManager;
    std::unique_ptr<AnimationManager> animationManager;
    std::unique_ptr<ScriptManager> scriptManager;
    std::unique_ptr<PhysicsManager> physicsManager;
    std::unique_ptr<ParticleManager> particleManager;

private:
    std::vector<Service*> services;
    std::vector<EventSubscription> m_eventSubscriptions;
    std::vector<std::function<void()>> frameCallbacks;
    bool isInitialized = false;

};
