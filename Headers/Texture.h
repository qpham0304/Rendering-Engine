#pragma once

#include <string>

class ShaderOpenGL;

class Texture
{
public:
	unsigned int ID;
	std::string type;
	std::string path;

public:
	virtual ~Texture() = default;

	virtual void Init(const char* path, const char* texType, bool flipUV) = 0;
	virtual void TexUnit(ShaderOpenGL& shader, const char* uniform, unsigned int unit) = 0;
	virtual void Bind() = 0;
	virtual void Unbind() = 0;
	virtual void Delete() = 0;

protected:
	Texture() = default;

	virtual void loadTexture(const char* path, bool flip) = 0;


protected:
	unsigned int unit = 0;

};