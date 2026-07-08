#pragma once

#include <sol/sol.hpp>

class Logger;

// engine functions that will be shared down to lua runtime scripting
class SharedFunctionsLua
{
public:
    SharedFunctionsLua(Logger* logger);

    void input(sol::state& luaState);
    void event(sol::state& luaState);
    void component(sol::state& luaState);
    void scene(sol::state& luaState);
    void logging(sol::state& luaState);
private:
    Logger* m_logger;

};