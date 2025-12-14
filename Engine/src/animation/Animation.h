#pragma once

#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <assimp/scene.h>
#include <functional>
#include "Bone.h"
#include "graphics/framework/OpenGL/core/ModelOpenGL.h"

struct AssimpNodeData
{
	glm::mat4 transformation;
	std::string name;
	int childrenCount;
	std::vector<AssimpNodeData> children;
};

class Animation
{
public:
	Animation();
	Animation(const std::string& animationPath, ModelOpenGL* model);
	~Animation();
	
	Bone* FindBone(const std::string& name);

	inline float GetTicksPerSecond() { 
		return static_cast<float>(m_TicksPerSecond); 
	}

	inline float GetDuration() { 
		return static_cast<float>(m_Duration); 
	}

	inline const AssimpNodeData& GetRootNode() { 
		return m_RootNode; 
	}

	inline const std::map<std::string, BoneInfo>& GetBoneIDMap() {
		return m_BoneInfoMap;
	}


private:
	void ReadMissingBones(const aiAnimation* animation, ModelOpenGL& model);

	void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src);
	double m_Duration = 0.0f;
	double m_TicksPerSecond;
	std::vector<Bone> m_Bones;	// joint animations
	AssimpNodeData m_RootNode;	// joint transformation
	std::map<std::string, BoneInfo> m_BoneInfoMap;
};

