#include "ScriptableCamera.h"
#include "window/AppWindow.h"
#include "core/components/MComponent.h"

ScriptableCamera::ScriptableCamera()
{

}

ScriptableCamera::~ScriptableCamera()
{

}

void ScriptableCamera::onUpdate()
{
	_setCameraData();
}

void ScriptableCamera::setProjection(glm::mat4 p)
{
	projection = p;
}

void ScriptableCamera::setView(glm::mat4 v)
{
	view = v;
}

void ScriptableCamera::setCamera(CameraComponent* cam)
{
	camera = cam;
}

void ScriptableCamera::_setCameraData()
{
	if(!camera) {
		return;
	}

	setViewWidth(camera->viewWidth);
    setViewHeight(camera->viewHeight);
    setProjection(camera->projection);
    setView(camera->view);
}
