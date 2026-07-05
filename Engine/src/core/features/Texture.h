#pragma once

#include <string>
#include <core/resources/Resource.h>

class Texture : public Resource
{
public:

public:
	virtual ~Texture() = default;

	virtual void Bind() = 0;
	virtual void Unbind() = 0;
	virtual void Delete() = 0;

	uint32_t id() { return m_id; }
	std::string path() { return m_path; }
	std::string type() { return m_type; }
	uint32_t width(){ return m_width; };
	uint32_t height(){ return m_height; };


protected:
	Texture() = default;
	Texture(uint32_t id) : m_id(id) {};

	virtual void loadTexture(const char* m_path, bool flip) = 0;

protected:
	uint32_t unit{ 0 };
	uint32_t m_id{ 0 };
	uint32_t m_width{ 0 };
	uint32_t m_height{ 0 };
	uint32_t m_numChannels{ 0 };

	std::string m_type{ "undefined" };
	std::string m_path{ "n/a" };
};