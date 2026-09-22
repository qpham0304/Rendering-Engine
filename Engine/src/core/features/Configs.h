#pragma once

#include "Engine/EngineCoreAPI.h"
#include <string>

enum class MYGRAPHICSENGINE_EXPORT GuiPlatform {
	UNDEFINED, IMGUI, QT
};

enum class MYGRAPHICSENGINE_EXPORT WindowPlatform {
	UNDEFINED, GLFW, SDL, Win32
};

enum class MYGRAPHICSENGINE_EXPORT RenderPlatform {
	UNDEFINED, OPENGL, VULKAN, DIRECTX,
};

enum class MYGRAPHICSENGINE_EXPORT LoggerPlatform {
	UNDEFINED, SPDLOG
};

enum class MYGRAPHICSENGINE_EXPORT OperatingSystem {
	UNDEFINED, WINDOW, LINUX, MACOS
};

enum class MYGRAPHICSENGINE_EXPORT ScriptingPlatform {
	UNDEFINED, LUA, MONO
};

enum class MYGRAPHICSENGINE_EXPORT PhysicsFramework {
	UNDEFINED, BOX3D, JOLT, PHYSX
};

struct MYGRAPHICSENGINE_EXPORT WindowConfig {
	std::string title = "Untitled";
	WindowPlatform windowPlatform = WindowPlatform::UNDEFINED;
	RenderPlatform renderPlatform = RenderPlatform::UNDEFINED;
	GuiPlatform guiPlatform = GuiPlatform::UNDEFINED;
	OperatingSystem os = OperatingSystem::UNDEFINED;
	ScriptingPlatform scriptingPlatform = ScriptingPlatform::LUA;
	PhysicsFramework physicsFramework = PhysicsFramework::BOX3D;
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

struct MYGRAPHICSENGINE_EXPORT GraphicsConfig {
	RenderPlatform platform;

};
