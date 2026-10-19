#pragma once

#include <sol/sol.hpp>
#include "core/events/Event.h"

class Logger;

// engine functions that will be shared down to lua runtime scripting
class SharedFunctionsLua
{
public:
    SharedFunctionsLua(Logger* logger);

    static void input(sol::state& luaState);
    static void scene(sol::state& luaState);
    static void logging(sol::state& luaState);
    
private:
    Logger* m_logger;


};