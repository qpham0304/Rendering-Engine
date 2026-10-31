#pragma once

#include "Engine/EngineCoreAPI.h"

// the state lives in the engine dll, header inline statics would give every module (game dll, tests) its own copy
class MYGRAPHICSENGINE_EXPORT EngineState {
public:
    enum class State {
        None = 0,
        Paused,
        Playing,
        Stopped
    };

    static void pause();
    static void play();
    static void stop();

    static bool isPaused();
    static bool isPlaying();
    static bool isStopped();
    static bool didJustResume();

    static State getState();
    static State getLastState();

private:
    static State s_currState;
    static State s_lastState;
};
