#pragma once

#include "graphics/framework/OpenGL/core/ShaderOpenGL.h"
#include <algorithm>
#include "../framework/OpenGL/core/skybox.h"

class Camera;

class SkyboxRenderer
{
private:
	void setup();
	std::vector<std::string> faces = {	// default skybox textures
		"assets/Textures/skybox/right.jpg",
		"assets/Textures/skybox/left.jpg",
		"assets/Textures/skybox/top.jpg",
		"assets/Textures/skybox/bottom.jpg",
		"assets/Textures/skybox/front.jpg",
		"assets/Textures/skybox/back.jpg"
	};

	Skybox skybox;
	ShaderOpenGL shaderProgram;

public:
	GLuint VAO;

	SkyboxRenderer();
	SkyboxRenderer(const char* path);
	~SkyboxRenderer();

	void setUniform();
	void updateTexture(const unsigned int& ID);
	unsigned int getTextureID();
	void render(Camera* camera);
	void render(Camera* camera, const unsigned int&);
	void free();
};

