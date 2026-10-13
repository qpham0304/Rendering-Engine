#include "SkeletonManager.h"

SkeletonManager::SkeletonManager()
    : Manager("SkeletonManager")
{

}

SkeletonManager::~SkeletonManager()
{

}

bool SkeletonManager::init(WindowConfig config)
{
    Service::init(config);

    return true;
}

bool SkeletonManager::onClose()
{
    return true;
}

void SkeletonManager::destroy(uint32_t id)
{
}

std::vector<uint32_t> SkeletonManager::listIDs() const
{
    std::vector<uint32_t> list;
    for(auto& [id, skeleton] : m_skeletons) {
        list.push_back(id);
    }
    return list;
}

void SkeletonManager::onUpdate()
{
    
}

uint32_t SkeletonManager::addSkeleton(std::string_view name)
{
    for(const auto& [id, skeleton] : m_skeletons) {
        if(skeleton->name == name) {
            return id;
        }
    }

    //TODO: might want to treat duplicate interval in the future
    m_skeletons[m_ids] = std::make_unique<SkeletonAsset>();
    m_skeletons[m_ids]->id = m_ids;
    m_skeletons[m_ids]->name = name;

    return _assignID();
}

std::shared_ptr<SkeletonManager::SkeletonAsset> SkeletonManager::getSkeleton(uint32_t id) const
{
    auto it = m_skeletons.find(id);
    if(it == m_skeletons.end()) {
        return nullptr;
    }
    return it->second;
}
