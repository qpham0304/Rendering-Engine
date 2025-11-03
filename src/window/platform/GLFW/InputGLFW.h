#pragma once
#include "../../src/window/Input.h"

class GLFWwindow;
class AppWindowGLFW;

class InputGLFW : public Input
{

public:
	friend AppWindowGLFW;

	InputGLFW();
	virtual ~InputGLFW() override;

	virtual bool isMousePressed(MouseCodes mouseCode) override;
	virtual bool isKeyPressed(KeyCodes keyCode) override;
	virtual int getMouseButton(MouseCodes mouseCode) override;
	virtual void getCursorPos(double* x, double* y) override;
	virtual int getKey(KeyCodes keyCode) override;

private:
	GLFWwindow* window{ nullptr };
};

