#pragma once

#include <stb/stb_image.h>
#include "Shader.h"
#include <string>

class Texture
{

private:
	void loadTexture(const char* path, bool flip);
public:
	unsigned int ID;
	std::string type;
	unsigned int unit = 0;
	std::string path;

	Texture() = default;
	Texture(const char* path, const char* texType);
	Texture(const char* fileName, const char* texType, const std::string& directory);

	//Texture(const Texture& other);
	//Texture& operator=(const Texture& other);
	//Texture(Texture&& other) noexcept;
	//Texture& operator=(Texture&& other) noexcept;

	~Texture();

	void Init(const char* path, const char* texType, bool flipUV);
	void TexUnit(Shader& shader, const char* uniform, unsigned int unit);
	void Bind();
	void Unbind();
	void Delete();
};