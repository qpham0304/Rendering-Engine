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

	// the renderer copies the poses into its frame's buffers after that frame's fence, onUpdate only touches the cpu side
	void uploadBoneMatrices(uint32_t frameIndex);
	uint32_t getAnimationBufferAddressID(uint32_t frameIndex) const;

	uint32_t addBoneMatrices(size_t boneCount);
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

	// one per frame in flight, a single copy would be rewritten while the gpu still draws the previous frame
	struct FrameBones {
		std::vector<uint32_t> bufferIDs;
		std::vector<uint64_t> addresses;
		uint32_t tableID { 0 };
	};
	std::vector<FrameBones> m_frameBones;
    std::vector<std::vector<glm::mat4>> m_boneMatricesData;

	std::unordered_map<std::string, AnimationData> m_animations;
    std::unordered_map<uint32_t, std::shared_ptr<Animator>> m_animators;
	bool _loadAnimation(std::string_view path);
};