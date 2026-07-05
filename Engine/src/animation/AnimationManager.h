#include "core/resources/managers/Manager.h"
#include "SpriteAnimator.h"
#include "SkinAnimator.h"

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

private:
	SpriteAnimator m_spriteAnimator;
	SkinAnimator m_skinAnimator;

};