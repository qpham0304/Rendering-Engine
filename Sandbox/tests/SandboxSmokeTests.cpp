#include <gtest/gtest.h>
#include "TestHelpers.h"
#include <core/Engine.h>
#include <core/layers/Layer.h>
#include <core/events/EventManager.h>
#include <core/scene/SceneManager.h>
#include <imgui.h>

// Boots the real engine (SDL window + Vulkan), loads the sandbox scene, runs a few frames and shuts down.
// Uses Sandbox/ as the working dir (WK_SANDBOX_DIR from Sandbox/tests/CMakeLists.txt), same as the dev build reading Sandbox/assets.
// In Debug, VMA asserts on leaked GPU allocations during teardown, which fails this test.

namespace {

constexpr int kFramesToRun = 60;

class SmokeTestLayer : public Layer {
public:
	SmokeTestLayer(int framesToRun, int& framesRun)
		: Layer("SmokeTestLayer"), m_framesToRun(framesToRun), m_framesRun(framesRun) {}

	bool init() override
	{
		ImGui::GetIO().IniFilename = nullptr;	// the smoke window is smaller, never overwrite Sandbox/assets/imgui.ini
		Scene* scene = SceneManager::getInstance().addScene("smoke-scene");
		m_sceneLoaded = scene->loadScene("assets/data/sandbox-scene.json");
		SceneManager::getInstance().setActiveScene(scene->getName());
		return true;
	}

	void onUpdate(const FrameTime& frameTime) override
	{
		if (++m_framesRun == m_framesToRun) {
			WindowCloseEvent closeEvent;	// same path as clicking the window's close button
			EventManager::getInstance().publish(closeEvent);
		}
	}

	void onDetach() override {}
	void onGuiUpdate() override {}
	void onEvent(Event& event) override {}

	bool sceneLoaded() const { return m_sceneLoaded; }

private:
	int m_framesToRun;
	int& m_framesRun;
	bool m_sceneLoaded { false };
};

}

TEST(EngineSmoke, BootLoadSceneRunFramesShutdown)
{
	ASSERT_TRUE(std::filesystem::exists(std::filesystem::path(WK_SANDBOX_DIR) / "assets/data/sandbox-scene.json"))
		<< "Sandbox assets not found under " << WK_SANDBOX_DIR;

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
	windowConfig.WorkDir = WK_SANDBOX_DIR;

	int framesRun = 0;
	SmokeTestLayer* layer = new SmokeTestLayer(kFramesToRun, framesRun);	// LayerManager owns and deletes it
	bool sceneLoaded = false;

	Engine app(windowConfig);
	app.pushLayer(layer);

	// report the exception message, then still close() so GPU resources are released before Engine is destroyed
	std::string failure;
	try {
		app.init();
		sceneLoaded = layer->sceneLoaded();
		app.start();
		app.run();
	}
	catch (const std::exception& e) {
		failure = e.what();
	}
	app.close();

	ASSERT_TRUE(failure.empty()) << "Engine threw: " << failure;

	EXPECT_TRUE(sceneLoaded);
	EXPECT_EQ(framesRun, kFramesToRun);
}

int main(int argc, char** argv)
{
	TestHelpers::disableCrashDialogs();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
