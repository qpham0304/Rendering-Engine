#pragma once

#include <sol/sol.hpp>

class Logger;

// engine functions that will be shared down to lua scripting
class SharedFunctionsLua
{
public:
    SharedFunctionsLua(Logger* logger);

    void input(sol::state& state);
    void event(sol::state& state);
    void component(sol::state& state);
    void logging(sol::state& state);
private:
    Logger* m_logger;

};