#pragma once

#include <gtest/gtest.h>
#include "TestHelpers.h"
#include <core/Engine.h>
#include <core/layers/Layer.h>
#include <core/events/EventManager.h>
#include <core/scene/SceneManager.h>
#include <core/entities/Entity.h>
#include <core/components/MComponent.h>
#include <window/InputKeyCode.h>
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Helpers for smoke tests that boot the real engine (SDL window + Vulkan).
// The engine registers its loggers by name in spdlog, so only one Engine can live in a process: tests that boot it
// more than once run each boot in a gtest death test child (EXPECT_EXIT), which also turns a crash into a failed test.

namespace SmokeTest {

inline WindowConfig makeWindowConfig(const std::string& workDir)
{
	WindowConfig windowConfig{};
	windowConfig.title = "Engine Smoke Test";
	windowConfig.windowPlatform = WindowPlatform::SDL;
	windowConfig.renderPlatform = RenderPlatform::VULKAN;
	windowConfig.guiPlatform = GuiPlatform::IMGUI;
	windowConfig.os = OperatingSystem::WINDOW;
	windowConfig.scriptingPlatform = ScriptingPlatform::LUA;
	windowConfig.physicsFramework = PhysicsFramework::BOX3D;
	windowConfig.width = 1280;
	windowConfig.height = 720;
	windowConfig.vsync = false;
	windowConfig.targetRenderFPS = 0.0f;	// uncapped, finish as fast as possible
	windowConfig.targetUpdateFPS = 60.0f;
	windowConfig.WorkDir = workDir;
	return windowConfig;
}

// an access violation would otherwise open the Windows "stopped working" dialog and hang the run
inline void disableErrorDialogs()
{
#ifdef _WIN32
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
}

// forward slashes, the model loader splits directories on '/'
inline std::string generic(const std::filesystem::path& path)
{
	return path.generic_string();
}

// The engine loggers live in the engine dll and write to the process stdout handle.
// Pointing that handle at a file before the Engine is created lets a test read back what was logged.
class LogCapture {
public:
	explicit LogCapture(std::filesystem::path file) : m_path(std::move(file))
	{
#ifdef _WIN32
		m_file = CreateFileW(m_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		m_previous = GetStdHandle(STD_OUTPUT_HANDLE);
		SetStdHandle(STD_OUTPUT_HANDLE, m_file);
#endif
	}

	~LogCapture()
	{
#ifdef _WIN32
		SetStdHandle(STD_OUTPUT_HANDLE, m_previous);
		CloseHandle(m_file);
#endif
	}

	std::string text() const
	{
		std::ifstream in(m_path, std::ios::binary);
		std::stringstream contents;
		contents << in.rdbuf();
		return contents.str();
	}

	size_t count(std::string_view needle) const
	{
		std::string log = text();
		size_t found = 0;
		for (size_t pos = log.find(needle); pos != std::string::npos; pos = log.find(needle, pos + needle.size())) {
			found++;
		}
		return found;
	}

	bool contains(std::string_view needle) const { return count(needle) > 0; }

private:
	std::filesystem::path m_path;
#ifdef _WIN32
	HANDLE m_file { INVALID_HANDLE_VALUE };
	HANDLE m_previous { nullptr };
#endif
};

// collects failures inside a death test child, where gtest assertions don't reach the parent; the exit code does
class Checks {
public:
	void expect(bool ok, const std::string& what)
	{
		if (!ok) {
			m_failures.push_back(what);
		}
	}

	int exitCode(const LogCapture& log) const
	{
		if (m_failures.empty()) {
			return 0;
		}
		for (const auto& failure : m_failures) {
			std::cerr << "CHECK FAILED: " << failure << "\n";
		}
		std::cerr << "---- engine log ----\n" << log.text() << std::endl;
		return 1;
	}

private:
	std::vector<std::string> m_failures;
};

// loads a scene (optional) and runs the given steps at fixed frame numbers, then closes the window
class ScriptedLayer : public Layer {
public:
	using Step = std::function<void(Scene&)>;

	ScriptedLayer(std::string scenePath, int closeAtFrame, int& framesRun)
		: Layer("ScriptedLayer"), m_scenePath(std::move(scenePath)), m_closeAtFrame(closeAtFrame), m_framesRun(framesRun) {}

	ScriptedLayer* at(int frame, Step step)
	{
		m_steps.push_back({ frame, std::move(step) });
		return this;
	}

	bool init() override
	{
		ImGui::GetIO().IniFilename = nullptr;	// never overwrite the work dir's imgui.ini
		m_scene = SceneManager::getInstance().addScene("smoke-scene");
		if (!m_scenePath.empty()) {
			m_scene->loadScene(m_scenePath);
		}
		SceneManager::getInstance().setActiveScene(m_scene->getName());
		return true;
	}

	void onUpdate(const FrameTime& frameTime) override
	{
		int frame = ++m_framesRun;
		for (auto& [stepFrame, step] : m_steps) {
			if (stepFrame == frame) {
				step(*m_scene);
			}
		}
		if (frame == m_closeAtFrame) {
			WindowCloseEvent closeEvent;	// same path as clicking the window's close button
			EventManager::getInstance().publish(closeEvent);
		}
	}

	void onDetach() override {}
	void onGuiUpdate() override {}
	void onEvent(Event& event) override {}

private:
	std::string m_scenePath;
	int m_closeAtFrame;
	int& m_framesRun;
	Scene* m_scene { nullptr };
	std::vector<std::pair<int, Step>> m_steps;
};

// returns the exception message that stopped the engine, empty when it ran and closed normally
inline std::string runEngine(const WindowConfig& config, Layer* layer)
{
	Engine app(config);
	app.pushLayer(layer);	// LayerManager owns and deletes it

	std::string failure;
	try {
		app.init();
		app.start();
		app.run();
	}
	catch (const std::exception& e) {
		failure = e.what();
	}
	app.close();
	return failure;
}

inline std::optional<Entity> findEntity(Scene& scene, const std::string& name)
{
	for (Entity e : scene.getEntitiesWith<NameComponent>()) {
		if (e.getComponent<NameComponent>().name == name) {
			return e;
		}
	}
	return std::nullopt;
}

// same path as a user pressing Ctrl+R, the scene reloads on its next update
inline void pressReload()
{
	KeyPressedEvent control(KEY_LEFT_CONTROL);
	KeyPressedEvent reload(KEY_R);
	EventManager::getInstance().publish(control);
	EventManager::getInstance().publish(reload);
}

}
