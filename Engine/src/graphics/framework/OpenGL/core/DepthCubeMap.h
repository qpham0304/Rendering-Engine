#pragma once

#include<glad/glad.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

class DepthCubeMap {
private:

public:
	const GLuint SHADOW_HEIGHT = 8192;
	const GLuint SHADOW_WIDTH = 8192;

	float near_plane = 1.0f, far_plane = 12.5f;

	GLuint FBO;
	GLuint texture;
	glm::mat4 lightProjection = glm::mat4(1.0f);
	glm::mat4 lightView;
	glm::mat4 lightSpaceMatrix;

	DepthCubeMap();
	void Bind();
	void Unbind();
	void Delete();
};

