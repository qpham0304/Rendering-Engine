#include "core/resources/managers/Manager.h"
#include "SpriteAnimator.h"
#include "SkinAnimator.h"

class BufferManager;

class AnimationManager : public Manager
{
public:
	AnimationManager();	
	virtual ~AnimationManager();

	virtual bool init(WindowConfig config) override;
    virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
    virtual void onUpdate() override;

	uint32_t getAnimationBufferAddressID();
	uint32_t addBoneMatrices(size_t boneCount);	//create gpu buffer memory to update bone matrices
	const std::vector<glm::mat4>& getFinalBoneMatrices(uint32_t id) const;

private:
    struct BoneMatricesData {
        uint32_t gpuBufferID { 0 }; //handle id to a buffer, not the actual direct gpu memory hence not uint64
        std::vector<glm::mat4> finalBoneMatrices;
    };

	SpriteAnimator m_spriteAnimator;
	SkinAnimator m_skinAnimator;

    std::vector<BoneMatricesData> m_boneMatricesBuffers;
    BufferManager* m_bufferManager{ nullptr };

	uint32_t deviceAdressBufferID { 0 };
};