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

enum class LoggerPlatform {
	UNDEFINED, SPDLOG
};

enum class OperatingSystem {
	UNDEFINED, WINDOW, LINUX, MACOS
};

enum class ScriptingPlatform {
	UNDEFINED, LUA, MONO
};

struct WindowConfig {
	std::string title = "Untitled";
	WindowPlatform windowPlatform = WindowPlatform::UNDEFINED;
	RenderPlatform renderPlatform = RenderPlatform::UNDEFINED;
	GuiPlatform guiPlatform = GuiPlatform::UNDEFINED;
	OperatingSystem os = OperatingSystem::UNDEFINED;
	ScriptingPlatform scriptingPlatform = ScriptingPlatform::LUA;
	int width = 1280;
	int height = 720;
	bool vsync = true;
	float targetRenderFPS = 60.0f;
	float targetUpdateFPS = 60.0f;
	std::string WorkDir = "./src";
	std::string AssetsDir = "./assets";
};

struct AppConfig {

};

struct GraphicsConfig {
	RenderPlatform platform;

};
