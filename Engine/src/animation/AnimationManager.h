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
	SpriteAnimator m_spriteAnimator;
	SkinAnimator m_skinAnimator;

    std::vector<uint64_t> m_boneMatricesBuffers;
    std::vector<std::vector<glm::mat4>> m_boneMatricesData;
    BufferManager* m_bufferManager{ nullptr };

	uint32_t deviceAdressBufferID { 0 };
};