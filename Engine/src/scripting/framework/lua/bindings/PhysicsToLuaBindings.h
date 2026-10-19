#pragma once

#include <sol/sol.hpp>

class PhysicsManager;

class PhysicsToLuaBindings
{
public:
    static void exportPhysics(sol::state& luaState, PhysicsManager* physicsManager);


private:
    static void _exportTypes(sol::state& luaState);
    static void _exportBodyFunctions(sol::state& luaState, PhysicsManager* physicsManager);

};
