#include "CameraComponent.h"
#include "core/features/Camera.h"

CameraComponent::CameraComponent(int width, int height, glm::vec3 position, glm::vec3 orientation)
{
    camera = new Camera();
    camera->init(width, height, position, orientation);
}

CameraComponent::~CameraComponent()
{
    delete camera;
}

glm::mat4 CameraComponent::projection()
{
    return camera->getProjectionMatrix();
}

glm::mat4 CameraComponent::view()
{
    return camera->getViewMatrix();
}
