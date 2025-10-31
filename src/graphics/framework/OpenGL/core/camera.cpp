#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../../src/window/Input.h"

Camera::Camera()
{
	width = DEFAULT_WIDTH;
	height = DEFAULT_HEIGHT;
	position = glm::vec3(0.0);
	Setup(width, height, position);
	SetupOrientation(orientation);
}

Camera::Camera(unsigned int width, unsigned int height, glm::vec3 position, glm::vec3 orientation)
{
	Setup(width, height, position);
	SetupOrientation(orientation);
}

Camera::Camera(unsigned int width, unsigned int height, glm::vec3 position)
{
	Setup(width, height, position);
}

void Camera::init(unsigned int width, unsigned int height, glm::vec3 position, glm::vec3 orientation)
{
	Setup(width, height, position);
	SetupOrientation(orientation);
}

void Camera::onUpdate()
{
	ReCalculateView();
	ReCalculateProjection();
	mvp = projection * view;
	right = glm::normalize(glm::cross(orientation, up));
	double currentFrame = glfwGetTime();
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

bool Camera::processKeyboard(GLFWwindow* window) {
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	bool isPressing = false;

	shiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
		|| glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		position += orientation * speed;
		isPressing = true;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		position -= orientation * speed;
		isPressing = true;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		position -= glm::normalize(glm::cross(orientation, up)) * speed;
		isPressing = true;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		position += glm::normalize(glm::cross(orientation, up)) * speed;
		isPressing = true;
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !shiftPressed) {
		position += glm::normalize(up) * speed;
		isPressing = true;
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && shiftPressed) {
		position -= glm::normalize(up) * speed;
		isPressing = true;
	}
	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && shiftPressed) {
		resetCamera();
		isPressing = true;
	}
	return isPressing;
}

bool Camera::processMouse(GLFWwindow* window) {

	bool isMouseMoved = false;

	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		isMouseMoved = true;
		mouseControl(window);
	}
	else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
		// Unhides cursor since camera is not looking around anymore
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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


void Camera::processInput(GLFWwindow* window)
{
	bool isMouseMoved = processMouse(window);
	bool isKeyboardMoved = processKeyboard(window);
	cameraMove = isMouseMoved || isKeyboardMoved;
}

void Camera::mouseControl(GLFWwindow* window)
{
	double x;
	double y;
	glfwGetCursorPos(window, &x, &y);

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

void Camera::mouse_callback(GLFWwindow* window, double x, double y)
{
	mouseControl(window);
}

void Camera::scroll_callback(double xoffset, double yoffset)
{
	fov -= (float)yoffset;
	if (fov < 1.0f)
		fov = 1.0f;
	if (fov > 45.0f)
		fov = 45.0f;
}

void Camera::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	processKeyboard(window);
}


void Camera::ReCalculateView()
{
	view = glm::lookAt(position, position + orientation, up);
	inView = glm::inverse(view);
}

void Camera::ReCalculateProjection()
{
	projection = glm::perspective(glm::radians(fov), (float)width / height, nearPlane, farPlane);
	inProjection = glm::inverse(projection);
}

void Camera::Setup(unsigned int& width, unsigned int& height, glm::vec3& position)
{
	this->width = width;
	this->height = height;
	this->position = position;
	this->defaultPosition = position;
	this->lastX = width / 2;
	this->lastY = height / 2;
	this->right = glm::cross(defaultUp, defaultOrientation);
}

void Camera::SetupOrientation(glm::vec3& orientation)
{
	if (orientation.x == 0.0)		// hack to avoid camera lock at 0.0;
		orientation.x = 0.01;
	this->defaultOrientation = orientation;
	this->orientation = orientation;
}