#include "RendererManager.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"

void RendererManager::addRenderTexture(std::string_view name, std::vector<uint32_t> textureIDs)
{
    auto it = m_renderTextures.find(name.data());
    if(it != m_renderTextures.end()) {
        m_logger->warn("found texture with given name: {} overwriting", name);
    }

    auto textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    m_renderTextures[name.data()].clear();
    for(auto& id : textureIDs) {
        auto texture = textureManager->getTextureShare(id);
        assert(texture && "addRendererTexture: texture not found");
        m_renderTextures[name.data()].push_back(texture);
    }
}

std::vector<std::shared_ptr<Texture>> RendererManager::getRenderTextures(std::string_view name)
{
    auto it = m_renderTextures.find(name.data());
    
    if(it != m_renderTextures.end()) {
        return it->second;
    }

    return {};
}
