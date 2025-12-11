#pragma once

#include "Manager.h"

class Texture;

class TextureManager : public Manager
{
public:
	virtual ~TextureManager() = default;

	virtual void init() = 0;
	virtual void shutdown() = 0;
	virtual void destroy(uint32_t id) = 0;
	virtual uint32_t loadTexture(std::string_view path) = 0;
	virtual uint32_t createDepthTexture() = 0;
	virtual Texture* getTexture(uint32_t id) {
		if (m_textures.find(id) == m_textures.end()) {
			return nullptr;
		}
		return m_textures[id].get();
	}

protected:
	// incremental resource so optimize by using with a vector for contiguous data is possible
	// but then managing destroy of resource could be a hassle so use map for now;
	std::unordered_map<uint32_t, std::shared_ptr<Texture>> m_textures;
	std::unordered_map<std::string, uint32_t> m_textureData;

	TextureManager() = default;
};

