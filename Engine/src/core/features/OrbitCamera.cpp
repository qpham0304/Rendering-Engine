#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "OrbitCamera.h"
#include "window/Input.h"
#include "window/AppWindow.h"

OrbitCamera::OrbitCamera() {
    width = DEFAULT_WIDTH;
    height = DEFAULT_HEIGHT;
    position = glm::vec3(0.0f, 0.0f, 5.0f);
    target = glm::vec3(0.0f);                       // center of rotation
    targetCenter = target;
    distance = 5.0f;
    targetDistance = distance;
    setup(width, height, position);
    setupOrientation(orientation);
}

OrbitCamera::OrbitCamera(unsigned int width, unsigned int height, glm::vec3 position, glm::vec3 orientation) {
    setup(width, height, position);
    setupOrientation(orientation);
    this->target = position + orientation * 5.0f; // guess a target based on orientation
    this->distance = 5.0f;
    this->targetCenter = target;
    this->targetDistance = distance;
}

OrbitCamera::OrbitCamera(unsigned int width, unsigned int height, glm::vec3 position) {
    setup(width, height, position);
    this->target = glm::vec3(0.0f);
    this->distance = glm::distance(position, target);
}

void OrbitCamera::init(unsigned int width, unsigned int height, glm::vec3 position, glm::vec3 orientation) {
    setup(width, height, position);
    setupOrientation(orientation);
    this->target = position + orientation * 5.0f;
    this->distance = 5.0f;
}

void OrbitCamera::setup(unsigned int& width, unsigned int& height, glm::vec3& position) {
    this->width = width;
    this->height = height;
    this->position = position;
    this->defaultPosition = position;
    this->lastX = (float)width / 2.0f;
    this->lastY = (float)height / 2.0f;
    this->right = glm::cross(defaultUp, defaultOrientation);
}

void OrbitCamera::setupOrientation(glm::vec3& orientation) {
    if (orientation.x == 0.0) {
        orientation.x = 0.01;   // camera lock avoidance hack
    }
    this->defaultOrientation = orientation;
    this->orientation = orientation;
}

void OrbitCamera::onUpdate() {
    double currentFrame = AppWindow::getTime();
    deltaTime = (float)(currentFrame - lastFrame);
    lastFrame = currentFrame;

    // framerate independent smoothing current Yaw/Pitch toward target Yaw/Pitch
    float interpolationFactor = 1.0f - glm::exp(-deltaTime / smoothTime);
    
    yaw += (targetYaw - yaw) * interpolationFactor;
    pitch += (targetPitch - pitch) * interpolationFactor;

    distance += (targetDistance - distance) * interpolationFactor;
    target += (targetCenter - target) * interpolationFactor;

    if (targetPitch > 89.0f) targetPitch = 89.0f;
    if (targetPitch < -89.0f) targetPitch = -89.0f;

    // smoothed yaw/pitch)
    float cosPitch = cos(glm::radians(pitch));
    float x = target.x + distance * cosPitch * cos(glm::radians(yaw));
    float y = target.y + distance * sin(glm::radians(pitch));
    float z = target.z + distance * cosPitch * sin(glm::radians(yaw));
    
    position = glm::vec3(x, y, z);
    orientation = glm::normalize(target - position);
    
    reCalculateView();
    reCalculateProjection();
    mvp = projection * view;

    // update directions for panning
    right = glm::normalize(glm::cross(orientation, glm::vec3(0, 1, 0)));
    up = glm::normalize(glm::cross(right, orientation));
}

void OrbitCamera::reCalculateView() {
    view = glm::lookAt(position, target, glm::vec3(0, 1, 0));
    inView = glm::inverse(view);
}

void OrbitCamera::reCalculateProjection() {
    if(height > 0) {
        projection = glm::perspective(glm::radians(fov), (float)width / height, nearPlane, farPlane);
    }
    inProjection = glm::inverse(projection);
}

bool OrbitCamera::processMouse() {
    bool leftPressed = AppWindow::isMousePressed(MOUSE_BUTTON_LEFT);
    
    if (leftPressed) {
        cameraMove = true;
        double x, y;
        AppWindow::getCursorPos(&x, &y);
        float xpos = static_cast<float>(x);
        float ypos = static_cast<float>(y);

        if (firstClick) {
            lastX = xpos;
            lastY = ypos;
            firstClick = false;
        }

        bool isPanning = AppWindow::isKeyPressed(KEY_LEFT_SHIFT);
        
        mouseControl(isPanning); 
        return true;
    } 
    else {
        cameraMove = false;
        firstClick = true; 
        return false;
    }
}

void OrbitCamera::mouseControl(bool panning) 
{
    double x, y;
    AppWindow::getCursorPos(&x, &y);

    float xpos = static_cast<float>(x);
    float ypos = static_cast<float>(y);

    float xOffset = (xpos - lastX) * sensitivity;
    float yOffset = (ypos - lastY) * sensitivity; 


    if (panning) {
        float panSpeed = distance * 0.001f; 
        targetCenter -= right * xOffset * panSpeed;
        targetCenter += up * yOffset * panSpeed;
    } else {    // prevents the camera from spinning 360 degrees in one flick
        sensitivity = 0.3f;
		smoothTime = 0.12f;
		float maxChange = 10.0f;                    // cap the camera speed
        targetYaw += glm::clamp(xOffset, -maxChange, maxChange);
        targetPitch += glm::clamp(yOffset, -maxChange, maxChange);
    }
    
    lastX = xpos;
    lastY = ypos;
}

bool OrbitCamera::processKeyboard() 
{
    bool isPressing = false;
    shiftPressed = AppWindow::isKeyPressed(KEY_LEFT_SHIFT) || AppWindow::isKeyPressed(KEY_RIGHT_SHIFT);
    float currentSpeed = speed * deltaTime * 100.0f;

    if (AppWindow::isKeyPressed(KEY_W)) { 
        targetCenter += orientation * currentSpeed;
        isPressing = true; 
    }
    if (AppWindow::isKeyPressed(KEY_S)) { 
        targetCenter -= orientation * currentSpeed;
        isPressing = true; 
    }
    if (AppWindow::isKeyPressed(KEY_A)) { 
        targetCenter -= right * currentSpeed;
        isPressing = true; 
    }
    if (AppWindow::isKeyPressed(KEY_D)) { 
        targetCenter += right * currentSpeed;
        isPressing = true; 
    }
    if (AppWindow::isKeyPressed(KEY_SPACE) && !shiftPressed) {
        targetCenter += glm::vec3(0,1,0) * currentSpeed;
        isPressing = true;
    }
    if (AppWindow::isKeyPressed(KEY_SPACE) && shiftPressed)  {
        targetCenter -= glm::vec3(0,1,0) * currentSpeed;
        isPressing = true;
    }
    if (AppWindow::isKeyPressed(KEY_R) && shiftPressed) { 
        resetCamera();
        isPressing = true;
    }
    cameraMove = isPressing;
    return isPressing;
}

void OrbitCamera::scroll_callback(double xoffset, double yoffset) 
{
    targetDistance -= static_cast<float>(yoffset) * (targetDistance * 0.1f);
    if (targetDistance < 0.5f) {
        targetDistance = 0.5f;
    }
    
    if (targetDistance > 100.0f) {
        targetDistance = 100.0f;
    }
}

void OrbitCamera::updateViewResize(int width, int height) 
{
    this->width = width;
    this->height = height;
}

void OrbitCamera::setCameraSpeed(int speedMultiplier) { 
    this->speedMultiplier = static_cast<float>(speedMultiplier); 
}

void OrbitCamera::translate(const glm::vec3& newPos) 
{
    glm::vec3 diff = newPos - position;
    target += diff;
    position = newPos;
}

void OrbitCamera::resetCamera() 
{
    position = defaultPosition;
    orientation = defaultOrientation;
    target = glm::vec3(0.0f);
    distance = glm::distance(position, target);
    targetCenter = glm::vec3(0.0f);
    targetDistance = glm::distance(defaultPosition, targetCenter);
    yaw = DEFAULT_YAW;
    pitch = DEFAULT_PITCH_;
    fov = DEFAULT_FOV;
}
