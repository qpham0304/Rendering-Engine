#include "CameraComponent.h"
#include "core/features/Camera.h"

CameraComponent::CameraComponent(int width, int height, glm::vec3 position, glm::vec3 orientation)
    : camera(width, height, position, orientation)
{
}

CameraComponent::~CameraComponent()
{
}

glm::mat4 CameraComponent::projection()
{
    return camera.getProjectionMatrix();
}

glm::mat4 CameraComponent::view()
{
    return camera.getViewMatrix();
}

glm::mat4 CameraComponent::invView()
{
    return camera.getInViewMatrix();
}

glm::mat4 CameraComponent::invProjection()
{
    return camera.getInProjectionMatrix();
}
