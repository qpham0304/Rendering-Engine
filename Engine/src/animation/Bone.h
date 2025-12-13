#pragma once

/* Container for bone data */

#include <vector>
#include <assimp/scene.h>
#include <list>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <assimp_glm_helpers.h>

#define GLM_ENABLE_EXPERIMENTAL

struct KeyPosition
{
	glm::vec3 position;
	float timeStamp{ 0.0f };
};

struct KeyRotation
{
	glm::quat orientation;
	float timeStamp { 0.0f };
};

struct KeyScale
{
	glm::vec3 scale;
	float timeStamp { 0.0f };
};

class Bone
{
public:
	Bone(const std::string& name, int ID, const aiNodeAnim* channel);
	void Update(float animationTime);
	glm::mat4 GetLocalTransform();
	std::string GetBoneName() const;
	int GetBoneID();
	int GetPositionIndex(float animationTime);
	int GetRotationIndex(float animationTime);
	int GetScaleIndex(float animationTime);



private:

	float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime);
	glm::mat4 InterpolatePosition(float animationTime);
	glm::mat4 InterpolateRotation(float animationTime);
	glm::mat4 InterpolateScaling(float animationTime);

	std::vector<KeyPosition> m_Positions;
	std::vector<KeyRotation> m_Rotations;
	std::vector<KeyScale> m_Scales;
	int m_NumPositions;
	int m_NumRotations;
	int m_NumScalings;

	glm::mat4 m_LocalTransform;
	std::string m_Name;
	int m_ID;
};

