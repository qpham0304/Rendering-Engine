#include <gtest/gtest.h>
#include "TestHelpers.h"
#include "SmokeTestSupport.h"
#include <core/features/EngineStates.h>
#include <core/scene/SceneManager.h>
#include <core/components/MComponent.h>
#include <nlohmann/json.hpp>

// play starts from a snapshot of the scene, pause freezes it, stop rebuilds the scene from the snapshot
// and the scripts init again on the next play. Each test boots the engine in its own process (EXPECT_EXIT)

namespace {

const glm::vec3 kStart { 0.0f, 6.0f, 0.0f };

std::string writePlayStopScene(const TestHelpers::TempDir& temp, const std::string& scriptPath)
{
	nlohmann::json faller = { { "components", {
		{ "NameComponent", { { "name", "Faller" } } },
		{ "ModelComponent", { { "path", "assets/models/cube/cube-notex.gltf" } } },
		{ "ColliderComponent", { { "bodyType", "Cube" }, { "colliderType", 2 }, { "shapeID", 0 } } },
		{ "ScriptComponent", { { "path", scriptPath } } },
		{ "ParticleEmitter", { { "emitRate", 50.0 } } },
		{ "TransformComponent", { { "translateVec", { kStart.x, kStart.y, kStart.z } } } },
	} } };

	nlohmann::json scene = { { "scene_name", "smoke-scene" }, { "entities", nlohmann::json::array({ faller }) } };
	return SmokeTest::generic(temp.write("scene.json", scene.dump(2)));
}

Entity findByName(Scene& scene, const std::string& name)
{
	for (Entity e : scene.getEntitiesWith<NameComponent>()) {
		if (e.getComponent<NameComponent>().name == name) {
			return e;
		}
	}
	return {};
}

int playPauseStopScenario()
{
	TestHelpers::TempDir temp;
	SmokeTest::LogCapture log(temp.path() / "engine.log");
	SmokeTest::Checks checks;

	const std::string script = SmokeTest::generic(temp.write("PlayStopProbe.lua", R"(
PlayStopProbe = {}
PlayStopProbe.__index = PlayStopProbe
function PlayStopProbe.new(entity) return setmetatable({ entity = entity }, PlayStopProbe) end
function PlayStopProbe:onInit()
    log_warn("PlayStopProbe init")
    getActiveScene():addEntity("spawned during play")
end
)"));
	std::string scenePath = writePlayStopScene(temp, script);

	auto fallerY = [](Scene& scene) {
		Entity faller = findByName(scene, "Faller");
		return faller.getRegistry() ? faller.getComponent<TransformComponent>().translateVec.y : -1000.0f;
	};
	auto entityCount = [](Scene& scene) { return scene.getEntitiesWith<NameComponent>().size(); };

	size_t editedCount = 0;
	float pausedY = 0.0f;
	int framesRun = 0;
	auto* layer = new SmokeTest::ScriptedLayer(scenePath, 260, framesRun);
	layer->at(5, [&](Scene& scene) {
		editedCount = entityCount(scene);
		EngineState::play();
	})->at(100, [&](Scene& scene) {
		checks.expect(fallerY(scene) < kStart.y - 0.001f, "the dynamic body falls while playing");
		checks.expect(entityCount(scene) == editedCount + 1, "the script spawned an entity on play");
		EngineState::pause();
		pausedY = fallerY(scene);
	})->at(140, [&](Scene& scene) {
		checks.expect(fallerY(scene) == pausedY, "pause freezes the body where it was");
		EngineState::stop();
	})->at(145, [&](Scene& scene) {
		Entity faller = findByName(scene, "Faller");
		checks.expect(faller.getRegistry() != nullptr, "stop rebuilds the scene's entities");
		checks.expect(fallerY(scene) == kStart.y, "stop restores the transform from before play");
		checks.expect(entityCount(scene) == editedCount, "stop removes the entity spawned during play");
		checks.expect(!faller.getComponent<ScriptComponent>().initilized, "the rebuilt script waits for the next play");
		checks.expect(faller.getComponent<ColliderComponent>().shapeID != 0, "the rebuilt collider has a body again");
	})->at(150, [&](Scene& scene) {
		EngineState::play();
	})->at(250, [&](Scene& scene) {
		checks.expect(fallerY(scene) < kStart.y - 0.001f, "the rebuilt body falls on the next play");
	});

	std::string failure = SmokeTest::runEngine(SmokeTest::makeWindowConfig(WK_SANDBOX_DIR), layer);

	checks.expect(failure.empty(), "engine threw: " + failure);
	checks.expect(framesRun == 260, "ran every frame, ran " + std::to_string(framesRun));
	checks.expect(log.count("PlayStopProbe init") == 2, "the script inits once per play, got " + std::to_string(log.count("PlayStopProbe init")));
	return checks.exitCode(log);
}

}

TEST(PlayStopSmoke, StopReloadsTheSceneAndPauseFreezesIt)
{
	EXPECT_EXIT(std::exit(playPauseStopScenario()), ::testing::ExitedWithCode(0), "");
}
