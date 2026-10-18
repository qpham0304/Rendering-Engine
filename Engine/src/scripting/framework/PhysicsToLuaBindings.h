#pragma once

#include <sol/sol.hpp>
#include "core/events/Event.h"

class PhysicsToLuaBindings 
{
public:
    static void exportPhysics(sol::state& luaState);


private:

};