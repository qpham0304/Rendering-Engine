#include "Animation.h"

Animation::Animation() = default;

Animation::Animation(const std::string& animationPath, ModelOpenGL* model)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(animationPath, aiProcess_Triangulate);
    assert(scene && scene->mRootNode);

    for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
        aiAnimation* animation = scene->mAnimations[i];
        std::cout << "Animation Name: " << i << " " << animation->mName.C_Str() << std::endl;
    }
    std::cout << "animations load success: " << scene->mNumAnimations << '\n';

    //TODO: hard coded for now, do something for multiple animations loading dynamically(swap animation at run time)
    aiAnimation* animation = nullptr;

    unsigned int index = 6;
    if (scene->mNumAnimations > index) {
        animation = scene->mAnimations[index];
    }

    else {
        animation = scene->mAnimations[0];
    }

    m_Duration = animation->mDuration;
    m_TicksPerSecond = animation->mTicksPerSecond;
    ReadHierarchyData(m_RootNode, scene->mRootNode);
    ReadMissingBones(animation, *model);
}

Animation::~Animation()
{

}

Bone* Animation::FindBone(const std::string& name)
{
    auto iter = std::find_if(m_Bones.begin(), m_Bones.end(),
        [&](const Bone& Bone)
        {
            return Bone.GetBoneName() == name;
        }
    );
    if (iter == m_Bones.end()) return nullptr;
    else return &(*iter);
}


void Animation::ReadMissingBones(const aiAnimation* animation, ModelOpenGL& model)
{
    int size = animation->mNumChannels;

    std::map<std::string, BoneInfo>& boneInfoMap = model.m_BoneInfoMap; //getting m_BoneInfoMap from Model class
    int& boneCount = model.GetBoneCount();      //getting the m_BoneCounter from Model class

    //reading channels(bones engaged in an animation and their keyframes)
    for (int i = 0; i < size; i++) {
        auto channel = animation->mChannels[i];
        std::string boneName = channel->mNodeName.data;

        if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
            boneInfoMap[boneName].id = boneCount;
            boneCount++;
        }
        m_Bones.push_back(Bone(channel->mNodeName.data, boneInfoMap[channel->mNodeName.data].id, channel));
    }

    m_BoneInfoMap = boneInfoMap;
}

void Animation::ReadHierarchyData(AssimpNodeData& dest, const aiNode* src)
{
    assert(src);

    dest.name = src->mName.data;
    dest.transformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(src->mTransformation);
    dest.childrenCount = src->mNumChildren;

    for (unsigned int i = 0; i < src->mNumChildren; i++) {
        AssimpNodeData newData;
        ReadHierarchyData(newData, src->mChildren[i]);
        dest.children.push_back(newData);
    }
}