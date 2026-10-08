#pragma once

#include "core/features/Math.h"
#include <vector>

class KeyFrame 
{
public:

private:
    float time;
    Math::VQS toParentFromBone;
};

class Track 
{
public:

private:
    std::vector<KeyFrame> keyFrames;
};

class Animation 
{
public:

private:
    float duration;
    std::vector<Track> tracks;
};
