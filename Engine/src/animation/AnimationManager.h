#pragma once

#include "core/resources/managers/Manager.h"
#include "Animation.h"
#include "SpriteAnimator.h"
#include "SkinAnimator.h"

struct aiScene;
class BufferManager;

typedef std::unordered_map<std::string, std::shared_ptr<Animation>> AnimationData;
class AnimationManager : public Manager
{
public:
	AnimationManager();	
	virtual ~AnimationManager();

	virtual bool init(WindowConfig config) override;
    virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
    virtual void onUpdate(const FrameTime& frameTime) override;

	uint32_t getAnimationBufferAddressID();
	
	uint32_t addBoneMatrices(size_t boneCount);	//create gpu buffer memory to update bone matrices
	std::vector<glm::mat4>& getFinalBoneMatrices(uint32_t id);
	
	bool loadAnimation(std::string_view path, const aiScene* scene);
	std::shared_ptr<Animation> getAnimation(std::string_view path, std::string_view name) const;
	const AnimationData* getAnimationData(std::string_view path);

	uint32_t addAnimator(uint32_t skeletonID);
	std::shared_ptr<Animator> getAnimator(uint32_t animatorID);

private:
	const int MAX_NUM_ANIMATIONS = 100;
    BufferManager* m_bufferManager{ nullptr };

	SpriteAnimator m_spriteAnimator;
	SkinAnimator m_skinAnimator;

	uint32_t deviceAdressBufferID { 0 };
    std::vector<uint64_t> m_boneMatricesBuffers;	//TODO: ideally uint32_t bufferID instead of direct memory address
    std::vector<std::vector<glm::mat4>> m_boneMatricesData;

	std::unordered_map<std::string, AnimationData> m_animations;
    std::unordered_map<uint32_t, std::shared_ptr<Animator>> m_animators;
	bool _loadAnimation(std::string_view path);
};