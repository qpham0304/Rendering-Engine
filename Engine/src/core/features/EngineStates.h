#pragma once

class EngineState {
public:
    enum class State {
        None = 0, 
        Paused, 
        Playing, 
        Stopped
    };

    static void pause() { 
        s_lastState = s_currState;
        s_currState = State::Paused; 
    }

    static void play() { 
        s_lastState = s_currState;
        s_currState = State::Playing; 
    }

    static void stop() { 
        s_lastState = s_currState;
        s_currState = State::Stopped; 
    }

    static bool isPaused() { return s_currState == State::Paused; }
    static bool isPlaying() { return s_currState == State::Playing; }
    static bool isStopped() { return s_currState == State::Stopped; }
    
    static bool didJustResume() {
        return (s_currState == State::Playing && s_lastState == State::Paused);
    }

    static State getLastState() { return s_lastState; }

private:
    static inline State s_currState = State::Stopped;
    static inline State s_lastState = State::Stopped;
};