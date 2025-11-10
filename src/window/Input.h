#pragma once

#include "InputKeyCode.h"

class AppWindow;

class Input 
{
public:
	virtual ~Input() = default;

	virtual bool isMousePressed(MouseCodes key) = 0;
	virtual bool isKeyPressed(KeyCodes key) = 0;;
	virtual int getMouseButton(MouseCodes key) = 0;
	virtual void getCursorPos(double* x, double* y) = 0;
	virtual int getKey(KeyCodes key) = 0;

protected:
	Input() = default;

};
