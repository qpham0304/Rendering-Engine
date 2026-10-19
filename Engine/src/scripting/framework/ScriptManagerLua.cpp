#include "ScriptManagerLua.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/entities/Entity.h"
#include "window/AppWindow.h"
#include "SharedFunctionsLua.h"
#include "EventToLuaBindings.h"
#include "MathToLuaBindings.h"
#include "PhysicsToLuaBindings.h"
#include "ComponentToLuaBindings.h"
#include "physics/PhysicsManager.h"


ScriptManagerLua::ScriptManagerLua(std::string serviceName)
    : ScriptManager(serviceName)
{

}

ScriptManagerLua::~ScriptManagerLua()
{
    
}

bool ScriptManagerLua::init(WindowConfig config)
{
    Service::init(config);
    
    //TODO: this won't work on dynamically defined components by the script
    // the serializer must be awared of the changes to deal with the new component

	m_luaState.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math);

    MathToLuaBindings::exportMath(m_luaState);
    EventToLuaBindings::exportEvent(m_luaState, m_logger.get());
    PhysicsToLuaBindings::exportPhysics(m_luaState, &ServiceLocator::GetService<PhysicsManager>("PhysicsManager"));
    
    SharedFunctionsLua shareToLua(m_logger.get()); 
    
    SharedFunctionsLua::input(m_luaState);
    SharedFunctionsLua::scene(m_luaState);
    SharedFunctionsLua::logging(m_luaState);

    ComponentToLuaBindings::exportComponents(m_luaState, m_logger.get());

    return true;
}

bool ScriptManagerLua::onClose()
{
    SceneManager& sceneManager = SceneManager::getInstance();
    for(uint32_t sceneID : sceneManager.listIDs()) {
        Scene* scene = sceneManager.getScene(sceneID);

        auto func = std::function<void(Entity)>([&](Entity entity) -> void {
            entity.getComponent<ScriptComponent>().onDestroy();
            entity.removeComponent<ScriptComponent>();
        });

        scene->forEnitiesWith<ScriptComponent>(func);
    }

    m_scripts.clear();
    m_scriptCache.clear();

    m_luaState["Entity"] = sol::nil;
    m_luaState.collect_garbage();

    return true;
}

void ScriptManagerLua::destroy(uint32_t id)
{

}

std::vector<uint32_t> ScriptManagerLua::listIDs() const
{
    std::vector<uint32_t> list;
    // for(const auto& [id, script] : m_scripts) {
    //     list.emplace_back(id);
    // }
    return list;
}

void ScriptManagerLua::onUpdate(const FrameTime& frameTime)
{
    std::vector<std::string> localReloadQueue;
    {
        auto lock = _lockWrite();
        if (!m_scriptsToReload.empty()) {
            localReloadQueue = std::move(m_scriptsToReload);
            m_scriptsToReload.clear();
        }
    }
    
    if (!localReloadQueue.empty()) {
        for (const auto& path : localReloadQueue) {
            _reloadScript(path);
        }
    }

    SceneManager& sceneManager = SceneManager::getInstance();
    Scene* scene = sceneManager.getActiveScene();

    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        auto& scriptComponent = entity.getComponent<ScriptComponent>();
        _initScript(scriptComponent);

        if(scriptComponent.onUpdate) {
            scriptComponent.onUpdate(frameTime.gameDeltaTime);
        }
    });

    scene->forEnitiesWith<ScriptComponent>(func);

}

void ScriptManagerLua::onFixedUpdate(float fixedDeltaTime)
{
    Scene* scene = SceneManager::getInstance().getActiveScene();
    if(!scene) {
        return;
    }

    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        auto& scriptComponent = entity.getComponent<ScriptComponent>();
        _initScript(scriptComponent);

        if(scriptComponent.onFixedUpdate) {
            scriptComponent.onFixedUpdate(fixedDeltaTime);
        }
    });

    scene->forEnitiesWith<ScriptComponent>(func);
}

void ScriptManagerLua::_initScript(ScriptComponent& scriptComponent)
{
    if(!scriptComponent.initilized) {
        scriptComponent.onInit();
        scriptComponent.initilized = true;
    }
}

void ScriptManagerLua::loadScript(Entity& entity, std::string_view path)
{
    std::string filePath(path);
    if(filePath.empty() || filePath == "None") {
        return;
    }

    try {
        if(!entity.hasComponent<ScriptComponent>()) {
            m_logger->error("ScriptComponent not found in entity {}", entity.getComponent<NameComponent>().name);
            return;
        }

        sol::table scriptClass;
        auto& scriptComponent = entity.getComponent<ScriptComponent>();

        auto it = m_scriptCache.find(filePath);
        if (it != m_scriptCache.end()) {
            scriptClass = it->second;
        } else {
            if (!std::filesystem::exists(filePath)) {
                m_logger->error("Script loading failed: File does not exist at path '{}'", filePath);
                return;
            }

            auto scriptResult = m_luaState.safe_script_file(filePath);
            if (!scriptResult.valid()) {
                sol::error err = scriptResult;
                m_logger->error("Failed to compile/execute Lua script file {}: {}", filePath, err.what());
                return;
            }
            
            std::string className = std::filesystem::path(filePath).stem().string(); 
            auto proxy = m_luaState[className];
            if(!proxy.valid()) {
                m_logger->error("Failed to find Lua class table: {}", className);
                m_logger->error("Make sure file name matches defined class name");
                return;
            }
            
            sol::table loadedClass = proxy;
            if (!loadedClass.valid()) {
                m_logger->error("Failed to validate Lua class table: {}", className);
                return;
            }

            m_scriptCache[filePath] = loadedClass;
            scriptClass = loadedClass;
        }

        sol::protected_function constructor = scriptClass["new"];
        if (!constructor.valid()) {
            std::string className = std::filesystem::path(filePath).stem().string();
            m_logger->error("Class {} does not have a 'new' constructor function!", className);
            return;
        }

        sol::protected_function_result result = constructor(Entity(entity));
        if (!result.valid()) {
            sol::error err = result;
            std::string className = std::filesystem::path(filePath).stem().string();
            m_logger->error("Failed to construct instance for Lua class {}: {}", className, err.what());
            return;
        }
        sol::table luaInstance = result;

        sol::protected_function initFunc = luaInstance["onInit"];
        if(initFunc.valid()) {
            if(scriptComponent.initilized) {
                scriptComponent.initilized = false;
            }
            scriptComponent.onInit = [luaInstance, initFunc]() mutable { initFunc(luaInstance); };
        } else {
            scriptComponent.onInit = [](){};
        }

        sol::protected_function updateFunc = luaInstance["onUpdate"];
        if(updateFunc.valid()) {
            scriptComponent.onUpdate = [luaInstance, updateFunc, this](double dt) mutable {
                auto result = updateFunc(luaInstance, dt); 
                if (!result.valid()) {
                    sol::error err = result;
                    m_logger->error("Lua script update error: {}", err.what());
                }
            };
        } else {
            scriptComponent.onUpdate = [](double){};
        }

        sol::protected_function fixedUpdateFunc = luaInstance["onFixedUpdate"];
        if(fixedUpdateFunc.valid()) {
            scriptComponent.onFixedUpdate = [luaInstance, fixedUpdateFunc, this](double dt) mutable {
                auto result = fixedUpdateFunc(luaInstance, dt);
                if (!result.valid()) {
                    sol::error err = result;
                    m_logger->error("Lua script fixed update error: {}", err.what());
                }
            };
        } else {
            scriptComponent.onFixedUpdate = [](double){};
        }

        sol::protected_function destroyFunc = luaInstance["onDestroy"];
        if(destroyFunc.valid()) {
            if(scriptComponent.onDestroy != nullptr) {
                scriptComponent.onDestroy();    // call clean up for script reload
            }
            scriptComponent.onDestroy = [luaInstance, destroyFunc]() mutable { destroyFunc(luaInstance); };
        } else {
            scriptComponent.onDestroy = [](){};
        }
    } catch (const std::runtime_error& e) {
        m_logger->error("{}", e.what());
    }
    
}

// on demand script calls, this will run the entire lua script file
// and cannot run selective function and no need to declare class
void ScriptManagerLua::loadScript(std::string_view path)
{
    sol::load_result fx = m_luaState.load_file(std::string(path));
    if (!fx.valid()) {
        sol::error err = fx;
        m_logger->error("failed to load script into the program {}", err.what());
    } else {
        m_scripts[std::string(path)] = fx.get<sol::protected_function>();
    }
}

void ScriptManagerLua::reloadScript(std::string_view p)
{
    auto lock = _lockWrite();
    m_scriptsToReload.emplace_back(p);
}

void ScriptManagerLua::runScript(std::string_view path)
{
    if (path == "None") {
        return;
    }

    auto it = m_scripts.find(std::string(path));
    if(it == m_scripts.end()) {
        m_logger->error("Cannot find script at: {}", std::string(path));
        return;
    }

    auto result = it->second();
    
    if (!result.valid()) {
        sol::error err = result;
        m_logger->error("Failed to run script at: {}, error: {}", std::string(path), err.what());
    }
}

void ScriptManagerLua::_reloadScript(const std::string& filePath)
{
    auto itGlobal = m_scripts.find(filePath);
    if (itGlobal != m_scripts.end()) {
        m_scripts.erase(itGlobal);
        loadScript(filePath);
        m_logger->info("Global script reloaded: {}", filePath);
        return; 
    }

    m_scriptCache.erase(filePath);

    SceneManager& sceneManager = SceneManager::getInstance();
    for(uint32_t sceneID : sceneManager.listIDs()) {
        Scene* scene = sceneManager.getScene(sceneID);

        auto func = std::function<void(Entity)>([&](Entity entity) -> void {
            auto& scriptComp = entity.getComponent<ScriptComponent>();
            
            if (scriptComp.path == filePath) {
                m_logger->info("Hot-reloading live entity instance utilizing script: {}", filePath);
                
                //TODO: be aware this currently does not reload entity or call onInit()
                loadScript(entity, filePath);
            }
        });

        scene->forEnitiesWith<ScriptComponent>(func);
    }

    // the reloaded instances are unreachable now, garbage collect so the subscriptions end before the next event
    m_luaState.collect_garbage();
}
