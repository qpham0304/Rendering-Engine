#pragma once
#include "window/Input.h"

struct SDL_Window;
class AppWindowSDL;

class InputSDL : public Input
{

public:
	friend AppWindowSDL;

	InputSDL();
	virtual ~InputSDL() override;

	virtual bool isMousePressed(MouseCodes mouseCode) override;
	virtual bool isKeyPressed(KeyCodes keyCode) override;
	virtual void getCursorPos(double* x, double* y) override;
	virtual void enableCursor() override;
	virtual void disableCursor() override;
	virtual int getMouseButton(MouseCodes mouseCode) override;
	virtual int getKey(KeyCodes keyCode) override;

	// engine key codes are GLFW's values, SDL works with scancodes
	static int toEngineKey(int sdlScancode);
	static int toSDLScancode(KeyCodes keyCode);

private:
	SDL_Window* m_windowHandle{ nullptr };

	// GLFW keeps reporting a moving cursor while it is disabled (relative mode), the cameras rely on that.
	// SDL stops moving the absolute position in relative mode, so the window accumulates it here instead
	double m_cursorX{ 0.0 };
	double m_cursorY{ 0.0 };
	bool m_relativeMode{ false };
};
