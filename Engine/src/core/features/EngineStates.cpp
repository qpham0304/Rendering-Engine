#include "EngineStates.h"

EngineState::State EngineState::s_currState = EngineState::State::Stopped;
EngineState::State EngineState::s_lastState = EngineState::State::Stopped;

void EngineState::pause()
{
    s_lastState = s_currState;
    s_currState = State::Paused;
}

void EngineState::play()
{
    s_lastState = s_currState;
    s_currState = State::Playing;
}

void EngineState::stop()
{
    s_lastState = s_currState;
    s_currState = State::Stopped;
}

bool EngineState::isPaused() { return s_currState == State::Paused; }
bool EngineState::isPlaying() { return s_currState == State::Playing; }
bool EngineState::isStopped() { return s_currState == State::Stopped; }

bool EngineState::didJustResume()
{
    return (s_currState == State::Playing && s_lastState == State::Paused);
}

EngineState::State EngineState::getState() { return s_currState; }
EngineState::State EngineState::getLastState() { return s_lastState; }
