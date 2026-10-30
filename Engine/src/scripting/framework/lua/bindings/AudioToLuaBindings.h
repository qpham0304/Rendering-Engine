#pragma once

#include <sol/sol.hpp>

class AudioManager;

class AudioToLuaBindings
{
public:
    static void exportAudio(sol::state& luaState, AudioManager* audioManager);

};
