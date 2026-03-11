#pragma once

#include <string>

class Texture
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
	unsigned int width(){ return m_width; };
	unsigned int height(){ return m_height; };


protected:
	Texture() = default;
	Texture(uint32_t id) : m_id(id) {};

	virtual void loadTexture(const char* m_path, bool flip) = 0;


protected:
	unsigned int unit{ 0 };
	unsigned int m_id{ 0 };
	unsigned int m_width{ 0 };
	unsigned int m_height{ 0 };

	std::string m_type{ "undefined" };
	std::string m_path{ "n/a" };
};