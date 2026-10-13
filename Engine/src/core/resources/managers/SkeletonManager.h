#pragma once

#include "Manager.h"
#include "core/features/Math.h"

class MYGRAPHICSENGINE_EXPORT SkeletonManager : public Manager
{
public:
    struct SkeletonNode {
		std::string name { "Unknown" };
        int parentIndex { -1 };
        Math::VQS vqs {};           //vec3 of position, scale, rotation
        glm::mat4 offsetMatrix {};
        glm::vec3 worldPosition { 0.0f }; // redundant, only for debug draw atm

		std::vector<uint32_t> childrenNodes{};

        bool isBone { false };
    };

    struct SkeletonAsset {
        uint32_t id { 0 };
        std::string name;
        uint32_t rootIndex = 0;

        std::vector<SkeletonNode> nodes;
        std::unordered_map<std::string, int> boneMap;
    };


    SkeletonManager();
	virtual ~SkeletonManager();

	virtual bool init(WindowConfig config) override;
    virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
    virtual void onUpdate() override;
    virtual uint32_t addSkeleton(std::string_view name);
    std::shared_ptr<SkeletonAsset> getSkeleton(uint32_t id) const;

private:
    std::unordered_map<uint32_t, std::shared_ptr<SkeletonAsset>> m_skeletons;
    
    std::vector<uint32_t> skeletonsTobeDestroyed;
};

