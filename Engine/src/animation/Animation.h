#pragma once

#include "core/features/Math.h"
#include <vector>

struct KeyPosition
{
	glm::vec3 position;
	float timeStamp;
};

struct KeyRotation
{
	Math::Quat orientation;
	float timeStamp;
};

struct KeyScale
{
	glm::vec3 scale;
	float timeStamp;
};


struct Track // all the keyframe data of a bone at each time stamp
{
    std::string boneName;
    std::vector<KeyPosition> keyPositions;
    std::vector<KeyRotation> keyRotations;
    std::vector<KeyScale> keyScales;
};

struct Animation 
{
    float ticksPerSecond{ 0.0f };
    float duration{ 0.0f };
    std::vector<Track> tracks;  //animated per bone info at each key frame
};

struct Animator
{
    //TODO: hard code getting animation
    std::string animationPath{ };
    std::string animationName{ };
    uint32_t skeletonID;
    float currentTime;
	float deltaTime;
};