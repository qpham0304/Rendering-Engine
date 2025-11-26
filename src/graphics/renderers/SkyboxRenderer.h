#pragma once

#include "src/graphics/framework/OpenGL/core/ShaderOpenGL.h"
#include <algorithm>
#include "../framework/OpenGL/core/skybox.h"

class Camera;

class SkyboxRenderer
{
private:
	void setup();
	std::vector<std::string> faces = {	// default skybox textures
		"Textures/skybox/right.jpg",
		"Textures/skybox/left.jpg",
		"Textures/skybox/top.jpg",
		"Textures/skybox/bottom.jpg",
		"Textures/skybox/front.jpg",
		"Textures/skybox/back.jpg"
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

