#pragma once

#include <vector>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <memory>
#include <ImGuizmo.h>
#include "./widgets/widget.h"

class GuiController {
private:

protected:
	std::vector<std::unique_ptr<Widget>> widgets;
	bool darkTheme = false;
	bool closeable = true;
	int width = 0;
	int height = 0;
	int count = 0;

public:
	virtual void init(GLFWwindow* window, int width, int height) = 0;
	virtual void start() = 0;
	virtual void render() = 0;
	virtual void end() = 0;
	virtual void onClose() = 0;

	virtual void setTheme(bool darkTheme) = 0;
	virtual void useLightTheme() = 0;
	virtual void useDarkTheme() = 0;
};

