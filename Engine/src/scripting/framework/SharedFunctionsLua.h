#pragma once

#include <sol/sol.hpp>
#include "core/events/Event.h"

class Logger;

// engine functions that will be shared down to lua runtime scripting
class SharedFunctionsLua
{
public:
    SharedFunctionsLua(Logger* logger);

    void math(sol::state& luaState);
    void input(sol::state& luaState);
    void event(sol::state& luaState);
    void component(sol::state& luaState);
    void scene(sol::state& luaState);
    void logging(sol::state& luaState);
    void physics(sol::state& luaState);
    
private:
    Logger* m_logger;

    template<typename T>
    void _registerEvent(sol::state& luaState) {
        auto ut = luaState.new_usertype<T>(T::GetStaticType(), 
            sol::base_classes, sol::bases<Event>()
        );

        // The visitor feeds field names and pointers directly to Sol3
        T::VisitFields([&ut](const char* name, auto memberPtr) {
            ut.set(name, memberPtr);
        });

        std::string castFunc = std::string("as_") + T::GetStaticType();
        luaState.set_function(castFunc, [] (Event& event) -> T* {
            if(event.GetName() == T::GetStaticType()) {
                return static_cast<T*>(&event);
            }
            return nullptr;
        });
    }

};