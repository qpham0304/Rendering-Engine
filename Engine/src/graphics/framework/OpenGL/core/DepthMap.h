#pragma once

#include<glad/glad.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

class DepthMap {
protected:
	void setup(unsigned int width, unsigned int height);
	const unsigned int DEFAULT_HEIGHT = 2048;
	const unsigned int DEFAULT_WIDTH = 2048;
	unsigned int width;
	unsigned int height;

public:
	unsigned int FBO;
	unsigned int texture;
	glm::mat4 lightProjection = glm::mat4(1.0f);
	glm::mat4 lightView;
	glm::mat4 lightSpaceMatrix;

	DepthMap();
	DepthMap(unsigned int width, unsigned int height);

	void Init(unsigned int width, unsigned int height);
	void Bind();
	void Unbind();
	void Delete();
	unsigned int getWidth();
	unsigned int getHeight();

};