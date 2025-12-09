#include "../../src/core/features/Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../../src/window/Input.h"
#include "../../src/window/AppWindow.h"

Camera::Camera()
{
	width = DEFAULT_WIDTH;
	height = DEFAULT_HEIGHT;
	position = glm::vec3(0.0);
	setup(width, height, position);
	setupOrientation(orientation);
}

Camera::Camera(unsigned int width, unsigned int height, glm::vec3 position, glm::vec3 orientation)
{
	setup(width, height, position);
	setupOrientation(orientation);
}

Camera::Camera(unsigned int width, unsigned int height, glm::vec3 position)
{
	setup(width, height, position);
}

void Camera::init(unsigned int width, unsigned int height, glm::vec3 position, glm::vec3 orientation)
{
	setup(width, height, position);
	setupOrientation(orientation);
}

void Camera::onUpdate()
{
	reCalculateView();
	reCalculateProjection();
	mvp = projection * view;
	right = glm::normalize(glm::cross(orientation, up));
	double currentFrame = AppWindow::getTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;
	speed = 2.5f * deltaTime * speedMultiplier;
}

void Camera::updateViewResize(int width, int height)
{
	this->width = width;
	this->height = height;
}

glm::vec3 Camera::getPosition()
{
	return position;
}

glm::vec3 Camera::getOrientation()
{
	return orientation;
}

glm::mat4 Camera::getViewMatrix()
{
	return view;
}

glm::mat4 Camera::getInViewMatrix()
{
	return inView;
}

glm::mat4 Camera::getProjectionMatrix()
{
	return projection;
}

glm::mat4 Camera::getInProjectionMatrix()
{
	return inProjection;
}

glm::mat4 Camera::getMVP()
{
	return mvp;
}

float Camera::getFOV()
{
	return fov;
}

int Camera::getViewWidth()
{
	return width;
}

int Camera::getViewHeight()
{
	return height;
}

bool Camera::isMoving()
{
	return cameraMove;
}

float Camera::getDeltaTime()
{
	return deltaTime;
}

bool Camera::processKeyboard() 
{
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	bool isPressing = false;

	shiftPressed = AppWindow::isKeyPressed(KEY_LEFT_SHIFT)
		|| AppWindow::isKeyPressed(KEY_RIGHT_SHIFT);

	if (AppWindow::isKeyPressed(KEY_W)) {
		position += orientation * speed;
		isPressing = true;
	}
	if (AppWindow::isKeyPressed(KEY_S)) {
		position -= orientation * speed;
		isPressing = true;
	}
	if (AppWindow::isKeyPressed(KEY_A)) {
		position -= glm::normalize(glm::cross(orientation, up)) * speed;
		isPressing = true;
	}
	if (AppWindow::isKeyPressed(KEY_D)) {
		position += glm::normalize(glm::cross(orientation, up)) * speed;
		isPressing = true;
	}
	if (AppWindow::isKeyPressed(KEY_SPACE) && !shiftPressed) {
		position += glm::normalize(up) * speed;
		isPressing = true;
	}
	if (AppWindow::isKeyPressed(KEY_SPACE) && shiftPressed) {
		position -= glm::normalize(up) * speed;
		isPressing = true;
	}
	if (AppWindow::isKeyPressed(KEY_R) && shiftPressed) {
		resetCamera();
		isPressing = true;
	}
	return isPressing;
}

bool Camera::processMouse() {

	bool isMouseMoved = false;

	if (AppWindow::isMousePressed(MOUSE_BUTTON_LEFT)) {
		isMouseMoved = true;
		mouseControl();
	}
	else if (!AppWindow::isMousePressed(MOUSE_BUTTON_LEFT)) {
		// Unhides cursor since camera is not looking around anymore
		// Makes sure the next time the camera looks around it doesn't jump
		firstClick = true;
		isMouseMoved = false;
	}
	return isMouseMoved;
}

void Camera::resetCamera()
{
	position = defaultPosition;
	orientation = defaultOrientation;
	speed = DEFAULT_SPEED;
	sensitivity = DEFAULT_SENSITIVITY;
	yaw = DEFAULT_YAW;
	pitch = DEFAULT_PITCH_;
	fov = DEFAULT_FOV;
	nearPlane = DEFAULT_NEARPLANE;
	farPlane = DEFAULT_FARPLANE;
}

void Camera::setCameraSpeed(int speedMultiplier)
{
	this->speedMultiplier = speedMultiplier;
}

void Camera::translate(const glm::vec3& position)
{
	this->position = position;
}


void Camera::processInput()
{
	bool isMouseMoved = processMouse();
	bool isKeyboardMoved = processKeyboard();
	cameraMove = isMouseMoved || isKeyboardMoved;
}

void Camera::mouseControl()
{
	double x;
	double y;
	AppWindow::getCursorPos(&x, &y);

	float xpos = static_cast<float>(x);
	float ypos = static_cast<float>(y);

	if (firstClick)
	{
		lastX = xpos;
		lastY = ypos;
		firstClick = false;
	}

	// Calculate change in mouse cursor position
	float xOffset = xpos - lastX;
	float yOffset = lastY - ypos; // Reversed since y-coordinates go from bottom to top

	// Update last mouse cursor position
	lastX = xpos;
	lastY = ypos;

	// Apply changes to camera orientation
	xOffset *= sensitivity;
	yOffset *= sensitivity;

	// Update camera orientation based on mouse movement
	// Example: Adjust yaw and pitch of the camera

	if(yaw == DEFAULT_YAW) {
		orientation = glm::normalize(orientation);
		pitch = glm::degrees(asin(orientation.y));
		yaw = glm::degrees(atan2(orientation.z, orientation.x));
	}
	else {
		yaw += xOffset;
		pitch += yOffset;
	}

	// Clamp pitch to prevent camera flipping
	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	// Update camera direction
	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	
	if (front.x == 0.0)	//prevent camera getting locked when x is 0.0
		front.x = 0.001;
	orientation = glm::normalize(front);
}

void Camera::scroll_callback(double xoffset, double yoffset)
{
	fov -= (float)yoffset;
	if (fov < 1.0f)
		fov = 1.0f;
	if (fov > 45.0f)
		fov = 45.0f;
}

void Camera::key_callback(int key, int scancode, int action, int mods) {
	processKeyboard();
}


void Camera::reCalculateView()
{
	view = glm::lookAt(position, position + orientation, up);
	inView = glm::inverse(view);
}

void Camera::reCalculateProjection()
{
	projection = glm::perspective(glm::radians(fov), (float)width / height, nearPlane, farPlane);
	inProjection = glm::inverse(projection);
}

void Camera::setup(unsigned int& width, unsigned int& height, glm::vec3& position)
{
	this->width = width;
	this->height = height;
	this->position = position;
	this->defaultPosition = position;
	this->lastX = width / 2;
	this->lastY = height / 2;
	this->right = glm::cross(defaultUp, defaultOrientation);
}

void Camera::setupOrientation(glm::vec3& orientation)
{
	if (orientation.x == 0.0)		// hack to avoid camera lock at 0.0;
		orientation.x = 0.01;
	this->defaultOrientation = orientation;
	this->orientation = orientation;
}