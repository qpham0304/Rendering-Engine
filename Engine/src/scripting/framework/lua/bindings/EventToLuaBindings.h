#pragma once

#include <sol/sol.hpp>
#include "core/events/Event.h"

class Logger;

class EventToLuaBindings 
{
public:
    static void exportEvent(sol::state& luaState, Logger* logger);


private:
    template<typename T>
    static void _registerEvent(sol::state& luaState) {
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