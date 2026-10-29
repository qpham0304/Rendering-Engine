#include "SmokeTestSupport.h"
#include <core/features/ServiceLocator.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/ModelManager.h>
#include <scripting/ScriptManager.h>
#include <nlohmann/json.hpp>

// bad assets must be logged and skipped, never crash the engine,
// and a fixed asset must load again on scene reload (Ctrl+R) or script reload.
// Each test boots the engine in its own process (EXPECT_EXIT), so a crash fails that test only.

namespace {

namespace fs = std::filesystem;

const std::string kTriangle =
	"v 0 0 0\nv 1 0 0\nv 0 1 0\n"
	"vt 0 0\nvt 1 0\nvt 0 1\n"
	"vn 0 0 1\n"
	"f 1/1/1 2/2/1 3/3/1\n";

std::string texturedTriangle(const std::string& mtlFile)
{
	return "mtllib " + mtlFile + "\nusemtl mat\n" + kTriangle;
}

nlohmann::json entity(const std::string& name, nlohmann::json components)
{
	components["NameComponent"] = { { "name", name } };
	return { { "components", components } };
}

std::string writeScene(const TestHelpers::TempDir& temp, const nlohmann::json& entities)
{
	nlohmann::json scene = { { "scene_name", "smoke-scene" }, { "entities", entities } };
	return SmokeTest::generic(temp.write("scene.json", scene.dump(2)));
}

TextureManager& textures() { return ServiceLocator::GetService<TextureManager>("TextureManagerVulkan"); }
ModelManager& models() { return ServiceLocator::GetService<ModelManager>("ModelManager"); }

bool hasLoadedModel(Scene& scene, const std::string& name)
{
	auto e = SmokeTest::findEntity(scene, name);
	return e && e->hasComponent<ModelComponent>() && models().getModel(e->getComponent<ModelComponent>().modelID) != nullptr;
}

uint32_t spriteTexture(Scene& scene, const std::string& name)
{
	auto e = SmokeTest::findEntity(scene, name);
	return (e && e->hasComponent<SpriteComponent>()) ? e->getComponent<SpriteComponent>().textureID : 0;
}

// copies only the compiled shaders, every other engine asset (fonts, hdr, default and noise textures) is missing
fs::path makeShaderOnlyWorkDir(const TestHelpers::TempDir& temp, const std::string& skipShader = "")
{
	fs::path workDir = temp.path() / "workdir";
	fs::path spvDir = workDir / "assets/shaders/spv";
	fs::create_directories(spvDir);
	for (const auto& entry : fs::directory_iterator(fs::path(WK_SANDBOX_DIR) / "assets/shaders/spv")) {
		if (entry.path().filename() != skipShader) {
			fs::copy_file(entry.path(), spvDir / entry.path().filename());
		}
	}
	return workDir;
}

// #1 #2 #3 #5 #9 #13 #14 and the stale collider shapeID
int badSceneAssetsScenario()
{
	TestHelpers::TempDir temp;
	SmokeTest::LogCapture log(temp.path() / "engine.log");
	SmokeTest::Checks checks;
	const std::string dir = SmokeTest::generic(temp.path());

	temp.write("triangle.obj", kTriangle);
	temp.write("missing_tex.obj", texturedTriangle("missing_tex.mtl"));
	temp.write("missing_tex.mtl", "newmtl mat\nmap_Kd does_not_exist.png\n");
	temp.write("corrupt_tex.obj", texturedTriangle("corrupt_tex.mtl"));
	temp.write("corrupt_tex.mtl", "newmtl mat\nmap_Kd corrupt.png\n");
	temp.write("corrupt.png", "this is a text file, not a png");
	temp.write("lines.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nl 1 2\nl 2 3\n");
	temp.write("garbage.fbx", "this is not a model");

	const std::string missingSprite = dir + "/missing_sprite.png";
	nlohmann::json entities = nlohmann::json::array({
		entity("Healthy", { { "ModelComponent", { { "path", dir + "/triangle.obj" } } } }),
		entity("MissingTextureModel", { { "ModelComponent", { { "path", dir + "/missing_tex.obj" } } } }),
		entity("CorruptTextureModel", { { "ModelComponent", { { "path", dir + "/corrupt_tex.obj" } } } }),
		entity("LineModel", { { "ModelComponent", { { "path", dir + "/lines.obj" } } } }),
		entity("GarbageModelA", { { "ModelComponent", { { "path", dir + "/garbage.fbx" } } } }),
		entity("GarbageModelB", { { "ModelComponent", { { "path", dir + "/garbage.fbx" } } } }),
		entity("MissingSpriteA", { { "SpriteComponent", { { "path", missingSprite } } } }),
		entity("MissingSpriteB", { { "SpriteComponent", { { "path", missingSprite } } } }),
		entity("ZeroSheetSprite", { { "SpriteComponent", { { "numRows", 0 }, { "numCols", 0 } } }, { "AnimationComponent", nlohmann::json::object() } }),
		entity("SpriteWithMissingModel", { { "ModelComponent", { { "path", dir + "/nope.obj" } } }, { "SpriteComponent", { { "path", missingSprite } } } }),
		entity("CameraOnly", { { "CameraComponent", nlohmann::json::object() } }),
		entity("StaleCollider", { { "ModelComponent", { { "path", dir + "/nope.obj" } } }, { "ColliderComponent", { { "shapeID", 7 } } } }),
	});
	// no TransformComponent on purpose, the json has one only when the editor saved it
	entities.push_back({ { "components", {
		{ "NameComponent", { { "name", "ColliderNoTransform" } } },
		{ "ModelComponent", { { "path", "$prim$Cube" } } },
		{ "ColliderComponent", { { "colliderType", 0 } } }
	} } });
	std::string scenePath = writeScene(temp, entities);

	int framesRun = 0;
	auto* layer = new SmokeTest::ScriptedLayer(scenePath, 30, framesRun);
	layer->at(5, [&](Scene& scene) {
		checks.expect(hasLoadedModel(scene, "Healthy"), "a good model in the same scene still loads");
		checks.expect(hasLoadedModel(scene, "MissingTextureModel"), "#1 a model whose texture is missing still loads");
		checks.expect(hasLoadedModel(scene, "CorruptTextureModel"), "#1 a model whose texture is corrupt still loads");
		checks.expect(!hasLoadedModel(scene, "LineModel"), "#3 a line only model is rejected");
		checks.expect(!hasLoadedModel(scene, "GarbageModelA") && !hasLoadedModel(scene, "GarbageModelB"), "a garbage model is rejected");

		uint32_t spriteA = spriteTexture(scene, "MissingSpriteA");
		checks.expect(spriteA == 0, "#1 a missing sprite texture gets the missing texture");
		checks.expect(spriteA == spriteTexture(scene, "MissingSpriteB"), "#14 the failed path is cached");

		// the component keeps its 0s, SpriteAnimator has divided by them every frame so far without crashing
		auto zeroSheet = SmokeTest::findEntity(scene, "ZeroSheetSprite");
		checks.expect(zeroSheet && zeroSheet->getComponent<SpriteComponent>().numRows == 0 && zeroSheet->getComponent<SpriteComponent>().numCols == 0,
			"#2 the 0 rows/cols sprite is animated without the component being changed");

		auto camera = SmokeTest::findEntity(scene, "CameraOnly");
		checks.expect(camera && camera->hasComponent<TransformComponent>(), "#5 camera without transform gets one");

		auto collider = SmokeTest::findEntity(scene, "ColliderNoTransform");
		checks.expect(collider && collider->hasComponent<TransformComponent>() && collider->getComponent<ColliderComponent>().shapeID != 0,
			"#5 collider without transform gets one and a body");

		auto stale = SmokeTest::findEntity(scene, "StaleCollider");
		checks.expect(stale && stale->getComponent<ColliderComponent>().shapeID == 0, "a saved shapeID without a body is cleared");
	});

	std::string failure = SmokeTest::runEngine(SmokeTest::makeWindowConfig(WK_SANDBOX_DIR), layer);

	checks.expect(failure.empty(), "engine threw: " + failure);
	checks.expect(framesRun == 30, "ran every frame, ran " + std::to_string(framesRun));
	checks.expect(log.count("failed to load texture image " + missingSprite) == 1, "#14 a missing texture is read and logged once");
	checks.expect(log.count("Model Loading failed: " + dir + "/garbage.fbx") == 1, "#14 a failed model is read and logged once");
	checks.expect(log.contains("has no triangle meshes"), "#3 the line only model is reported");
	checks.expect(log.contains("does_not_exist.png"), "#1 the missing material texture is reported");
	checks.expect(log.contains("Collider on entity 'StaleCollider' skipped"), "#13 collider without model goes through the logger");
	return checks.exitCode(log);
}

// #10 #14: Ctrl+R after failed loads does not crash, and assets fixed on disk load on the next reload
int reloadPicksUpFixedAssetsScenario()
{
	TestHelpers::TempDir temp;
	SmokeTest::LogCapture log(temp.path() / "engine.log");
	SmokeTest::Checks checks;
	const std::string dir = SmokeTest::generic(temp.path());
	const std::string lateTexture = dir + "/late.png";

	std::string scenePath = writeScene(temp, nlohmann::json::array({
		entity("LateModel", { { "ModelComponent", { { "path", dir + "/late.obj" } } } }),
		entity("LateSpriteA", { { "SpriteComponent", { { "path", lateTexture } } } }),
		entity("LateSpriteB", { { "SpriteComponent", { { "path", lateTexture } } } }),
		entity("Crate", { { "ModelComponent", { { "path", "$prim$Cube" } } }, { "ColliderComponent", { { "colliderType", 0 } } } }),
	}));

	int framesRun = 0;
	auto* layer = new SmokeTest::ScriptedLayer(scenePath, 25, framesRun);
	layer->at(3, [&](Scene& scene) {
		checks.expect(!hasLoadedModel(scene, "LateModel"), "missing model is not loaded");
		checks.expect(spriteTexture(scene, "LateSpriteA") == 0, "missing sprite texture uses the fallback");
		SmokeTest::pressReload();
	});
	layer->at(8, [&](Scene& scene) {
		checks.expect(SmokeTest::findEntity(scene, "LateModel").has_value(), "#10 the scene reloaded after failed loads");
		checks.expect(!hasLoadedModel(scene, "LateModel"), "model still missing after the first reload");

		temp.write("late.obj", kTriangle);
		fs::copy_file(fs::path(WK_SANDBOX_DIR) / "assets/Textures/default/32x32/albedo.png", temp.path() / "late.png");
		SmokeTest::pressReload();
	});
	layer->at(13, [&](Scene& scene) {
		checks.expect(hasLoadedModel(scene, "LateModel"), "#14 a model fixed on disk loads on reload");
		uint32_t texture = spriteTexture(scene, "LateSpriteA");
		checks.expect(texture != 0, "#14 a texture fixed on disk loads on reload");
	});

	std::string failure = SmokeTest::runEngine(SmokeTest::makeWindowConfig(WK_SANDBOX_DIR), layer);

	checks.expect(failure.empty(), "engine threw: " + failure);
	checks.expect(framesRun == 25, "ran every frame, ran " + std::to_string(framesRun));
	checks.expect(log.count("failed to load texture image " + lateTexture) == 2, "#14 the missing texture is tried once per load, not once per sprite");
	return checks.exitCode(log);
}

// #11 #12: lua errors in onInit / onUpdate / onDestroy are logged once with the script path, a hot reload brings the script back
int luaErrorsScenario()
{
	TestHelpers::TempDir temp;
	SmokeTest::LogCapture log(temp.path() / "engine.log");
	SmokeTest::Checks checks;

	const std::string initScript = SmokeTest::generic(temp.write("InitError.lua", R"(
InitError = {}
InitError.__index = InitError
function InitError.new(entity) return setmetatable({ entity = entity }, InitError) end
function InitError:onInit() error("init boom") end
function InitError:onUpdate(dt) log_warn("InitError update ran") end
function InitError:onDestroy() error("destroy boom") end
)"));
	const std::string updateScript = SmokeTest::generic(temp.write("UpdateError.lua", R"(
UpdateError = {}
UpdateError.__index = UpdateError
function UpdateError.new(entity) return setmetatable({ entity = entity }, UpdateError) end
function UpdateError:onUpdate(dt) error("update boom") end
)"));

	std::string scenePath = writeScene(temp, nlohmann::json::array({
		entity("InitErrorEntity", { { "ScriptComponent", { { "path", initScript } } } }),
		entity("UpdateErrorEntity", { { "ScriptComponent", { { "path", updateScript } } } }),
	}));

	int framesRun = 0;
	size_t updateErrorsBeforeReload = 0;
	auto* layer = new SmokeTest::ScriptedLayer(scenePath, 45, framesRun);
	layer->at(30, [&](Scene&) {
		updateErrorsBeforeReload = log.count("onUpdate error in " + updateScript);
		temp.write("UpdateError.lua", R"(
UpdateError = {}
UpdateError.__index = UpdateError
function UpdateError.new(entity) return setmetatable({ entity = entity }, UpdateError) end
function UpdateError:onUpdate(dt) log_warn("UpdateError fixed") end
)");
		ServiceLocator::GetService<ScriptManager>("ScriptManager").reloadScript(updateScript);
	});

	std::string failure = SmokeTest::runEngine(SmokeTest::makeWindowConfig(WK_SANDBOX_DIR), layer);

	checks.expect(failure.empty(), "engine threw: " + failure);
	checks.expect(framesRun == 45, "ran every frame, ran " + std::to_string(framesRun));
	checks.expect(log.count("onInit error in " + initScript) == 1, "#11 onInit error is logged with the script path");
	checks.expect(!log.contains("InitError update ran"), "#12 a script that failed onInit is disabled");
	checks.expect(updateErrorsBeforeReload == 1, "#12 onUpdate error is logged once, not every frame (got " + std::to_string(updateErrorsBeforeReload) + ")");
	checks.expect(log.count("UpdateError fixed") >= 5, "a fixed script runs again after hot reload");
	checks.expect(log.contains("onDestroy error in " + initScript), "#11 onDestroy error is logged");
	return checks.exitCode(log);
}

// #1 (engine textures) #4 #7: with only the shaders on disk the engine still starts, renders and closes
int shadersOnlyScenario()
{
	TestHelpers::TempDir temp;
	SmokeTest::LogCapture log(temp.path() / "engine.log");
	SmokeTest::Checks checks;
	fs::path workDir = makeShaderOnlyWorkDir(temp);

	std::string scenePath = writeScene(temp, nlohmann::json::array({
		entity("Cube", { { "ModelComponent", { { "path", "$prim$Cube" } } } }),
		entity("Sprite", { { "SpriteComponent", { { "path", "assets/textures/mobi-padoru.png" } } } }),
	}));

	int framesRun = 0;
	auto* layer = new SmokeTest::ScriptedLayer(scenePath, 30, framesRun);
	std::string failure = SmokeTest::runEngine(SmokeTest::makeWindowConfig(workDir.string()), layer);

	checks.expect(failure.empty(), "engine threw: " + failure);
	checks.expect(framesRun == 30, "ran every frame, ran " + std::to_string(framesRun));
	checks.expect(log.contains("failed to load, using a flat sky color"), "#4 missing environment map falls back");
	checks.expect(log.contains("Font not found"), "#7 missing font falls back");
	checks.expect(log.contains("obluenoise256.png"), "#1 missing blue noise is reported");
	checks.expect(log.contains("DirtMaskTexture.jpg"), "#1 missing lens dirt is reported");
	checks.expect(log.contains("albedo.png"), "#1 missing material fallback texture is reported");
	return checks.exitCode(log);
}

// #6 #8: a missing shader stops the engine with a readable error and a clean teardown (no VMA leak assert)
int missingShaderScenario()
{
	TestHelpers::TempDir temp;
	SmokeTest::LogCapture log(temp.path() / "engine.log");
	SmokeTest::Checks checks;
	fs::path workDir = makeShaderOnlyWorkDir(temp, "HiZ.comp.spv");

	int framesRun = 0;
	auto* layer = new SmokeTest::ScriptedLayer("", 30, framesRun);
	std::string failure = SmokeTest::runEngine(SmokeTest::makeWindowConfig(workDir.string()), layer);

	checks.expect(failure.find("HiZ.comp.spv") != std::string::npos, "exception names the missing shader, got: " + failure);
	checks.expect(framesRun == 0, "no frame ran");
	checks.expect(log.contains("Renderer HiZPassVulkan failed to init"), "#6 the failing renderer is logged");
	checks.expect(log.contains("Engine init failed"), "#8 the error is logged before teardown");
	return checks.exitCode(log);
}

}

TEST(AssetFailureSmoke, BadSceneAssetsAreLoggedAndSkipped)
{
	EXPECT_EXIT(std::exit(badSceneAssetsScenario()), ::testing::ExitedWithCode(0), "");
}

TEST(AssetFailureSmoke, SceneReloadPicksUpFixedAssets)
{
	EXPECT_EXIT(std::exit(reloadPicksUpFixedAssetsScenario()), ::testing::ExitedWithCode(0), "");
}

TEST(AssetFailureSmoke, LuaErrorsAreLoggedOnceAndRecoverOnReload)
{
	EXPECT_EXIT(std::exit(luaErrorsScenario()), ::testing::ExitedWithCode(0), "");
}

TEST(AssetFailureSmoke, EngineRunsWithOnlyShadersOnDisk)
{
	EXPECT_EXIT(std::exit(shadersOnlyScenario()), ::testing::ExitedWithCode(0), "");
}

TEST(AssetFailureSmoke, MissingShaderStopsCleanly)
{
	EXPECT_EXIT(std::exit(missingShaderScenario()), ::testing::ExitedWithCode(0), "");
}
