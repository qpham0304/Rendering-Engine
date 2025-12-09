#include "InputGLFW.h"
#include <GLFW/glfw3.h>
#include "../../AppWindow.h"
#include "AppWindowGLFW.h"

InputGLFW::InputGLFW() : Input()
{

}

InputGLFW::~InputGLFW()
{

}

bool InputGLFW::isMousePressed(MouseCodes mouseCode)
{
	return glfwGetMouseButton(m_windowHandle, static_cast<int>(mouseCode)) == GLFW_PRESS;
}

bool InputGLFW::isKeyPressed(KeyCodes keyCode)
{
	return glfwGetKey(m_windowHandle, static_cast<int>(keyCode)) == GLFW_PRESS;
}

int InputGLFW::getMouseButton(MouseCodes mouseCode)
{
	return glfwGetMouseButton(m_windowHandle, static_cast<int>(mouseCode));
}

void InputGLFW::getCursorPos(double* x, double* y)
{
	return glfwGetCursorPos(m_windowHandle, x, y);
}

int InputGLFW::getKey(KeyCodes keyCode)
{
	return glfwGetKey(m_windowHandle, static_cast<int>(keyCode));
}
