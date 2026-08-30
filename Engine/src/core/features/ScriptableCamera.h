#pragma once

#include <glm/glm.hpp>
#include "Camera.h"
#include <functional>

class CameraComponent;

class ScriptableCamera : public Camera
{
protected:

public:
	ScriptableCamera();
	virtual ~ScriptableCamera() override;

	virtual void onUpdate() override;
	virtual void resetCamera() override {};
	virtual void translate(const glm::vec3& position) override {};

	virtual bool processKeyboard() override { return true; };
	virtual bool processMouse() override { return true; };
	virtual void scroll_callback(double xoffset, double yoffset) override {};

	void setProjection(glm::mat4 p);
	void setView(glm::mat4 v);
	void setCamera(CameraComponent* cam);

private:
	CameraComponent* camera { nullptr };

	void _setCameraData();

};
