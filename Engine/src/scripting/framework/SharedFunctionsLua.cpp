#include "SharedFunctionsLua.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/entities/Entity.h"
#include "window/AppWindow.h"
#include "logging/logger.h"

SharedFunctionsLua::SharedFunctionsLua(Logger* logger)
    : m_logger(logger)
{

}

void SharedFunctionsLua::input(sol::state &state)
{

}

void SharedFunctionsLua::event(sol::state &state)
{

}

void SharedFunctionsLua::component(sol::state &m_luaState)
{
    m_luaState.new_usertype<NameComponent>("NameComponent",
        "name", &NameComponent::name
    );
}

void SharedFunctionsLua::logging(sol::state &state)
{
    Logger& clientLogger = ServiceLocator::GetService<Logger>("Client_LoggerSPD");

    state["log_error"] = [&](const std::string& msg) { clientLogger.error(msg); };
    state["log_info"] = [&](const std::string& msg) { clientLogger.info(msg); };
    state["log_warn"] = [&](const std::string& msg) { clientLogger.warn(msg); };
    state["log_critical"] = [&](const std::string& msg) { clientLogger.critical(msg); };
}
