#include "RendererManager.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"

void RendererManager::addRenderTexture(std::string_view name, uint32_t textureID)
{
    auto it = m_renderTextures.find(name.data());
    if(it != m_renderTextures.end()) {
        m_logger->error("found texture with given name: {} overwriting", name);
    }

    auto textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    m_renderTextures[name.data()] = textureManager->getTextureShare(textureID);
}

std::shared_ptr<Texture> RendererManager::getRenderTexture(std::string_view name)
{
    auto it = m_renderTextures.find(name.data());
    
    if(it != m_renderTextures.end()) {
        return it->second;
    }

    return nullptr;
}
