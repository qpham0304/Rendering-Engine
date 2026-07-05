#include "ScriptManagerLua.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/entities/Entity.h"
#include "window/AppWindow.h"
#include "SharedFunctionsLua.h"

using namespace entt::literals;

namespace LuaJsonBridge {
    // convert nlohmann::json object into native sol::table
    inline sol::object jsonToLua(sol::state_view lua, const nlohmann::json& j) {
        if (j.is_null()) return sol::nil;
        if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
        if (j.is_number_integer()) return sol::make_object(lua, j.get<int64_t>());
        if (j.is_number_float()) return sol::make_object(lua, j.get<double>());
        if (j.is_string()) return sol::make_object(lua, j.get<std::string>());
        
        if (j.is_array()) {
            sol::table t = lua.create_table();
            int index = 1; // Lua array index start at 1... why?
            for (const auto& item : j) {
                t[index++] = jsonToLua(lua, item);
            }
            return t;
        }
        
        if (j.is_object()) {
            sol::table t = lua.create_table();
            for (auto it = j.begin(); it != j.end(); ++it) {
                t[it.key()] = jsonToLua(lua, it.value());
            }
            return t;
        }
        return sol::nil;
    }

    // convert native sol::table back into nlohmann::json object
    inline nlohmann::json luaToJson(sol::object obj) {
        if (obj.is<sol::nil_t>()) return nullptr;
        if (obj.is<bool>()) return obj.as<bool>();
        if (obj.is<int64_t>()) return obj.as<int64_t>();
        if (obj.is<double>()) return obj.as<double>();
        if (obj.is<std::string>()) return obj.as<std::string>();
        
        if (obj.is<sol::table>()) {
            sol::table t = obj.as<sol::table>();
            
            // Check if this table functions as an array (has numeric keys)
            bool is_array = false;
            size_t len = t.size();
            if (len > 0) {
                is_array = true;
                for (size_t i = 1; i <= len; ++i) {
                    if (t[i] == sol::nil) { is_array = false; break; }
                }
            }
            
            if (is_array) {
                nlohmann::json j = nlohmann::json::array();
                for (size_t i = 1; i <= len; ++i) {
                    j.push_back(luaToJson(t[i]));
                }
                return j;
            } else {
                nlohmann::json j = nlohmann::json::object();
                t.for_each([&j](sol::object key, sol::object value) {
                    if (key.is<std::string>()) {
                        j[key.as<std::string>()] = luaToJson(value);
                    }
                });
                return j;
            }
        }
        return nullptr;
    }
}

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
    
    //TODO: this won't work on dynamically added components, serializer must be awared of the changes

	m_luaState.open_libraries(sol::lib::base, sol::lib::package);

    SharedFunctionsLua shareToLua(m_logger.get()); 
    
    shareToLua.input(m_luaState);
    shareToLua.event(m_luaState);
    shareToLua.component(m_luaState);
    shareToLua.logging(m_luaState);
    
    Serializer serializer;
    auto componentFactory = serializer.getComponentFactory();
    auto componentDestroyer = serializer.getComponentDestroyer();
    m_luaState.new_usertype<Entity>("Entity",
        "getComponent", [this](Entity& entity, std::string_view componentName) -> sol::object {
            entt::meta_type metaType;
            for (auto&& [id, type] : entt::resolve()) {
                auto prop = type.prop("name"_hs);
                if (prop) {
                    // assumes component is registered in the serializer's macro
                    if (prop.value().cast<std::string>() == componentName) {
                        metaType = type;
                        break;
                    }
                }
            }

            if (!metaType) {
                m_logger->error("Lua requested component '{}' which doesn't exist in C++ EnTT reflection!", componentName);
                return sol::nil;
            }

            // fetch the raw pointer to the component on this entity using its underlying type ID
            auto* storage = entity.getRegistry()->storage(metaType.id());
            if (!storage || !storage->contains(entity)) {
                return sol::nil;
            }
            
            const void* componentPtr = storage->value(entity);

            // invoke SerializerInternal::Serialize function dynamically over the raw pointer
            auto serializeFunc = metaType.func("serialize"_hs);
            if (!serializeFunc) {
                return sol::nil;
            }

            entt::meta_any result = serializeFunc.invoke({}, componentPtr);
            if (auto* jsonPtr = result.try_cast<nlohmann::json>()) {
                return LuaJsonBridge::jsonToLua(m_luaState, *jsonPtr);
            }

            return sol::nil;
        },
        "setComponent", [this, componentFactory](Entity& entity, std::string_view componentName, sol::object luaTable) {
            nlohmann::json updatedData = LuaJsonBridge::luaToJson(luaTable);
            
            auto it = componentFactory.find(std::string(componentName));
            if (it != componentFactory.end()) {
                // invoke the factory lambda registered via REGISTER_COMPONENT
                // this replaces the C++ data and runs the matching lifecycle hooks
                it->second(entity, updatedData);
            } else {
                m_logger->error("Failed to update component: '{}' factory not registered.", componentName);
            }
        },
        "addComponent", [this, componentFactory](Entity& entity, std::string_view componentName, sol::object luaTable) {
            nlohmann::json updatedData = LuaJsonBridge::luaToJson(luaTable);

            auto it = componentFactory.find(std::string(componentName));
            if (it != componentFactory.end()) {
                it->second(entity, updatedData);
            } else {
                m_logger->error("Failed to add component: '{}' factory not registered.", componentName);
            }
        },
        "removeComponent", [this, componentDestroyer](Entity& entity, std::string_view componentName) {
            auto it = componentDestroyer.find(std::string(componentName));
            if (it != componentDestroyer.end()) {
                it->second(entity);
            } else {
                m_logger->error("Failed to add component: '{}' factory not registered.", componentName);
            }
        }
    );

    return true;
}

bool ScriptManagerLua::onClose()
{


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

void ScriptManagerLua::onUpdate()
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

    // for(auto& path : m_scriptsToReload) {
    //     _reloadScript(path);
    // }
    // m_scriptsToReload.clear();

    SceneManager& sceneManager = SceneManager::getInstance();
    for(uint32_t sceneID : sceneManager.listIDs()) {
        Scene* scene = sceneManager.getScene(sceneID);

        auto func = std::function<void(Entity)>([&](Entity entity) -> void {
            auto& scriptComponent = entity.getComponent<ScriptComponent>();
            
            if(!scriptComponent.initilized) {
                scriptComponent.onInit();
                scriptComponent.initilized = true;
            }

            float dt = AppWindow::getDeltaTime();
            if(scriptComponent.onUpdate) {
                scriptComponent.onUpdate(dt);
            }
        });

        scene->forEnitiesWith<ScriptComponent>(func);
    }

}

void ScriptManagerLua::loadScript(Entity& entity, std::string_view path)
{
    if(!entity.hasComponent<ScriptComponent>()) {
        m_logger->error("ScriptComponent not found in entity {}", entity.getComponent<NameComponent>().name);
        return;
    }

    auto& scriptComponent = entity.getComponent<ScriptComponent>();

    sol::table scriptClass;
    std::string filePath(path);
    if(filePath.empty() || filePath == "None") {
        return;
    }

    auto it = m_scriptCache.find(filePath);
    if (it != m_scriptCache.end()) {
        scriptClass = it->second;
    } else {
        if (!std::filesystem::exists(filePath)) {
            m_logger->error("Script loading failed: File does not exist at path '{}'", filePath);
            return;
        }

        auto scriptResult = m_luaState.script_file(filePath);
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

    sol::protected_function destroyFunc = luaInstance["onDestroy"];
    if(destroyFunc.valid()) {
        scriptComponent.onDestroy = [luaInstance, destroyFunc]() mutable { destroyFunc(luaInstance); };
    } else {
        scriptComponent.onDestroy = [](){};
    }
}

// on demand script calls, this will run the entire lua script file
// and cannot run selective function and no need to declare class
void ScriptManagerLua::loadScript(std::string_view path)
{
    sol::load_result fx = m_luaState.load_file(std::string(path));
	if (!fx.valid()) {
		sol::error err = fx;
		m_logger->error("failed to load string-based script into the program {}", err.what());
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

    auto itCache = m_scriptCache.find(filePath);
    if (itCache == m_scriptCache.end()) {
        m_logger->warn("No script to reload at: {}", filePath);
        return;
    }

    m_scriptCache.erase(itCache);

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
}
