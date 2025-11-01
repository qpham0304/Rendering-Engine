#pragma once

#include <string>

enum class GuiPlatform {
	UNDEFINED, IMGUI, QT
};

enum class WindowPlatform {
	UNDEFINED, GLFW, SDL, Win32
};

enum class RenderPlatform {
	UNDEFINED, OPENGL, VULKAN, DIRECTX,
};

struct WindowConfig {
	std::string title = "Untitled";
	RenderPlatform renderPlatform = RenderPlatform::UNDEFINED;
	WindowPlatform windowPlatform = WindowPlatform::UNDEFINED;
	int width = 1280;
	int height = 720;
	bool vsync = true;
};

struct AppConfig {

};

struct GraphicsConfig {
	RenderPlatform platform;

};
