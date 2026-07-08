#include "SharedFunctionsLua.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/entities/Entity.h"
#include "window/AppWindow.h"
#include "logging/logger.h"
#include <tuple>

SharedFunctionsLua::SharedFunctionsLua(Logger* logger)
    : m_logger(logger)
{

}

void SharedFunctionsLua::input(sol::state& luaState )
{
    luaState["isMousePressed"] = &AppWindow::isMousePressed;
    luaState["isKeyPressed"] = &AppWindow::isKeyPressed;
    luaState["getMouseButton"] = &AppWindow::getMouseButton;
    luaState["getCursorPos"] = [] () {
        double x, y;
        AppWindow::getCursorPos(&x, &y);
        return std::make_tuple(x, y);
    };
    luaState["enableCursor"] = &AppWindow::enableCursor;
    luaState["disableCursor"] = &AppWindow::disableCursor;
    luaState["getKey"] = &AppWindow::getKey;
    luaState["getTime"] = &AppWindow::getTime;
}

void SharedFunctionsLua::event(sol::state& luaState )
{

}

void SharedFunctionsLua::component(sol::state& luaState )
{
    luaState.new_usertype<NameComponent>("NameComponent",
        "name", &NameComponent::name
    );
}

void SharedFunctionsLua::scene(sol::state& luaState )
{
    luaState["getActiveScene"] = [] () -> Scene* {
        SceneManager& sceneManager = SceneManager::getInstance();
        return sceneManager.getActiveScene();
    };

    luaState.new_usertype<Scene>("Scene",
        "addEntity", &Scene::addEntity,
        "removeEntity", sol::overload(
            sol::resolve<bool(const std::string&)>(&Scene::removeEntity),
            sol::resolve<bool(const uint32_t& )>(&Scene::removeEntity)
        ),
        "getName", &Scene::getName
    );

}

void SharedFunctionsLua::logging(sol::state& luaState )
{
    Logger& clientLogger = ServiceLocator::GetService<Logger>("Client_LoggerSPD");

    luaState["log_error"] = [&](const std::string& msg) { clientLogger.error(msg); };
    luaState["log_info"] = [&](const std::string& msg) { clientLogger.info(msg); };
    luaState["log_warn"] = [&](const std::string& msg) { clientLogger.warn(msg); };
    luaState["log_critical"] = [&](const std::string& msg) { clientLogger.critical(msg); };
}
