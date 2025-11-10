#pragma once

#include<stb/stb_image.h>
#include"Shader.h"

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

	//// Copy constructor
	//Texture(const Texture& other);
	//// Copy assignment operator
	//Texture& operator=(const Texture& other);

	//Texture(Texture&& other) noexcept;

	//Texture& operator=(Texture&& other) noexcept;

	~Texture();

	void Init(const char* path, const char* texType, bool flipUV);
	void texUnit(Shader& shader, const char* uniform, unsigned int unit);
	void Bind();
	void Unbind();
	void Delete();

	static void BIND_ALBEDO();
	static void BIND_NORMAL();
	static void BIND_METALLIC();
	static void BIND_ROUGHNESS();
	static void BIND_AO();
	static void BIND_IRRADIANCE();
	static void BIND_PREFILTER();
	static void BIND_BRDF_LUT_TEXTURE();
	static void BIND_HEIGHT();
	static void BIND_SHADOW();
};