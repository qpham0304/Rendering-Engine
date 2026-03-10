#pragma once

#include "Manager.h"

class Texture;

class TextureManager : public Manager
{
public:
	TextureManager(std::string serviceName = "TextureManager") : Manager(serviceName) {};
	virtual ~TextureManager() = default;

	virtual bool init(WindowConfig config) = 0;
	virtual bool onClose() = 0;
	virtual void destroy(uint32_t id) = 0;
	virtual std::vector<uint32_t> listIDs() const override {
		std::vector<uint32_t> list;
		for(const auto& [id, texture] : m_textures) {
			list.emplace_back(id);
		}
		return list;
	}
	virtual uint32_t loadTexture(std::string_view path, uint32_t mipLevels = 1, bool isDataTexture = false) = 0;
	virtual uint32_t createTexture() = 0;
	virtual uint32_t createDepthTexture(uint32_t width, uint32_t height, uint32_t mipLevels = 1) = 0;
	virtual Texture* getTexture(uint32_t id) {
		if (m_textures.find(id) == m_textures.end()) {
			return nullptr;
		}
		return m_textures[id].get();
	}

	// the input id is the id of raw texture managed by the conrete classes
	// the returned id is the id to the inspectable texture i.e: descriptorset for vulkan
	virtual void* inspectTexture(uint32_t id) = 0;

protected:
	// these are internal managed texture by the concrete classes
	std::unordered_map<uint32_t, std::shared_ptr<Texture>> m_textures;
	std::unordered_map<std::string, uint32_t> m_textureData;
	std::unordered_map<uint32_t, uint32_t> textureIDs;	// this is the proxy texture to be inspected 
};

