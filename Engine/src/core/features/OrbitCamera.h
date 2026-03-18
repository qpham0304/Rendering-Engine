#pragma once

#include <glm/glm.hpp>
#include "Camera.h"

class OrbitCamera : public Camera
{
protected:
	float deltaTime = 0.0f;	// Time between current frame and last frame
	float lastFrame = 0.0f; // Time of last frame

	void reCalculateView();
	void reCalculateProjection();
	void setup(unsigned int& width, unsigned int& height, glm::vec3& position);
	void setupOrientation(glm::vec3& orientation);
		
	float targetYaw = 0.0f;
	float targetPitch = 0.0f;
	float smoothTime = 0.08f;
	
	glm::vec3 target = glm::vec3(0.0f); 
	float distance = 10.0f;

public:
	OrbitCamera();
	OrbitCamera(unsigned int width, unsigned int height, glm::vec3 position, glm::vec3 orientation);
	OrbitCamera(unsigned int width, unsigned int height, glm::vec3 position);
	virtual ~OrbitCamera() override = default;

	virtual void init(unsigned int width, unsigned int height, glm::vec3 position, glm::vec3 orientation) override;
	virtual void onUpdate() override;
	virtual void updateViewResize(int width, int height) override;
	virtual void processInput() override;
	virtual void resetCamera() override;
	virtual void setCameraSpeed(int speedMultiplier) override;
	virtual void translate(const glm::vec3& position) override;

	virtual bool processKeyboard() override;
	virtual bool processMouse() override;
	virtual void scroll_callback(double xoffset, double yoffset) override;
	virtual void key_callback(int key, int scancode, int action, int mods) override;

private:
	void mouseControl(bool panning);

};
