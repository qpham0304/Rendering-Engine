#pragma once

#include <glm/glm.hpp>

class Camera;

class CameraComponent
{
private:
	Camera* camera;

public:
	CameraComponent() : camera(nullptr) {};
	CameraComponent(int width, int height, glm::vec3 position, glm::vec3 orientation);
	~CameraComponent();

	glm::mat4 projection();
	glm::mat4 view();
	glm::mat4 invView();
	glm::mat4 invProjection();
};

