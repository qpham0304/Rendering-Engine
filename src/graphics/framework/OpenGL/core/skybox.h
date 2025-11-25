#pragma once
#include <glad/glad.h>
#include <stb/stb_image.h>
#include <vector>
#include <string>
#include <iostream>

class Skybox
{
public:
	Skybox() = default;
	Skybox(std::vector<std::string> faces);
	~Skybox();

	void init(std::vector<std::string> faces);
	void updateTexture(const unsigned int& id);
	unsigned int textureID();
	void free();

private:
	unsigned int ID;
	unsigned int loadCubeMap(std::vector<std::string> faces);
};

