#include <gtest/gtest.h>
#include "TestHelpers.h"
#include "core/scene/Scene.h"
#include "core/entities/Entity.h"
#include "core/components/MComponent.h"

// Scene JSON loading, including the bad-input cases from notes/ASSET_LOADING_BUGS.md that are already handled.
// Only components with no load hook (Name, Transform, Prefab, Ribbon) are used, so no GPU managers are needed.

namespace {

class SceneLoadingTest : public ::testing::Test {
protected:
	TestHelpers::TempDir temp;
	Scene scene{ "test-scene" };

	// entities only move from "new" to the scene's map on the next update
	void flush() { scene.onUpdate(0.0f); }

	size_t entityCount() { return scene.getEntitiesWith<NameComponent>().size(); }

	Entity findByName(const std::string& name)
	{
		for (Entity e : scene.getEntitiesWith<NameComponent>()) {
			if (e.getComponent<NameComponent>().name == name) {
				return e;
			}
		}
		ADD_FAILURE() << "no entity named " << name;
		return {};
	}
};

}

TEST_F(SceneLoadingTest, ValidSceneLoadsEveryEntity)
{
	std::string path = temp.write("scene.json", R"({
		"scene_name": "test-scene",
		"entities": [
			{ "components": { "NameComponent": { "name": "A" }, "TransformComponent": { "translateVec": [1, 2, 3] } } },
			{ "components": { "NameComponent": { "name": "B" } } }
		]
	})");

	ASSERT_TRUE(scene.loadScene(path));
	EXPECT_EQ(entityCount(), 2u);
	EXPECT_EQ(findByName("A").getComponent<TransformComponent>().translateVec, glm::vec3(1, 2, 3));
}

TEST_F(SceneLoadingTest, MissingFileFails)
{
	EXPECT_FALSE(scene.loadScene(temp.file("does-not-exist.json")));
	EXPECT_EQ(entityCount(), 0u);
}

TEST_F(SceneLoadingTest, InvalidJsonFails)
{
	std::string path = temp.write("broken.json", R"({ "scene_name": "x", "entities": [ )");
	EXPECT_FALSE(scene.loadScene(path));
	EXPECT_EQ(entityCount(), 0u);
}

TEST_F(SceneLoadingTest, MissingSceneNameFails)
{
	std::string path = temp.write("noname.json", R"({ "entities": [] })");
	EXPECT_FALSE(scene.loadScene(path));
}

TEST_F(SceneLoadingTest, EntitiesNotAnArrayLoadsNothing)
{
	std::string path = temp.write("object.json", R"({ "scene_name": "x", "entities": { "oops": 1 } })");
	EXPECT_TRUE(scene.loadScene(path));
	EXPECT_EQ(entityCount(), 0u);
}

TEST_F(SceneLoadingTest, UnknownComponentIsSkippedOthersStillLoad)
{
	std::string path = temp.write("unknown.json", R"({
		"scene_name": "x",
		"entities": [ { "components": { "NameComponent": { "name": "A" }, "MadeUpComponent": { "value": 1 } } } ]
	})");

	ASSERT_TRUE(scene.loadScene(path));
	EXPECT_EQ(entityCount(), 1u);
	EXPECT_EQ(findByName("A").getComponent<NameComponent>().name, "A");
}

TEST_F(SceneLoadingTest, WrongValueTypeSkipsThatComponentOnly)
{
	std::string path = temp.write("badtype.json", R"({
		"scene_name": "x",
		"entities": [ { "components": { "NameComponent": { "name": "A" }, "TransformComponent": { "translateVec": "nope" } } } ]
	})");

	ASSERT_TRUE(scene.loadScene(path));
	Entity a = findByName("A");
	EXPECT_FALSE(a.hasComponent<TransformComponent>());
}

TEST_F(SceneLoadingTest, MissingPrefabStillCreatesEntity)
{
	std::string path = temp.write("prefab.json", R"({
		"scene_name": "x",
		"entities": [ { "prefab_ref": "no/such/prefab.json", "overrides": { "components": { "NameComponent": { "name": "P" } } } } ]
	})");

	ASSERT_TRUE(scene.loadScene(path));
	Entity p = findByName("P");
	EXPECT_TRUE(p.hasComponent<PrefabComponent>());
}

TEST_F(SceneLoadingTest, PrefabOverridesArePatchedOnTop)
{
	std::string prefab = temp.write("crate.prefab.json", R"({
		"components": { "NameComponent": { "name": "Crate" }, "TransformComponent": { "scaleVec": [2, 2, 2] } }
	})");

	nlohmann::json sceneJson = {
		{ "scene_name", "x" },
		{ "entities", { {
			{ "prefab_ref", prefab },
			{ "overrides", { { "components", { { "TransformComponent", { { "translateVec", { 5, 0, 0 } } } } } } } }
		} } }
	};
	std::string path = temp.write("scene.json", sceneJson.dump());

	ASSERT_TRUE(scene.loadScene(path));
	auto& transform = findByName("Crate").getComponent<TransformComponent>();
	EXPECT_EQ(transform.scaleVec, glm::vec3(2.0f));					// from the prefab
	EXPECT_EQ(transform.translateVec, glm::vec3(5.0f, 0.0f, 0.0f));	// from the override
}

TEST_F(SceneLoadingTest, ChildrenGetParentRelationship)
{
	std::string path = temp.write("children.json", R"({
		"scene_name": "x",
		"entities": [ {
			"components": { "NameComponent": { "name": "Parent" } },
			"children": [
				{ "components": { "NameComponent": { "name": "Child1" } } },
				{ "components": { "NameComponent": { "name": "Child2" } } }
			]
		} ]
	})");

	ASSERT_TRUE(scene.loadScene(path));
	Entity parent = findByName("Parent");
	Entity child = findByName("Child1");

	ASSERT_TRUE(parent.hasComponent<RelationshipComponent>());
	EXPECT_EQ(parent.getComponent<RelationshipComponent>().children.size(), 2u);
	EXPECT_EQ(child.getComponent<RelationshipComponent>().parent, static_cast<entt::entity>(parent));
}

TEST_F(SceneLoadingTest, EntityWithoutTransformGetsDefaultOne)
{
	GTEST_SKIP() << "Known bug #5 in notes/ASSET_LOADING_BUGS.md: hooks assume a TransformComponent exists. "
					"Remove this skip once EntitySerializer::deserializeEntity adds a default one.";

	std::string path = temp.write("notransform.json", R"({
		"scene_name": "x",
		"entities": [ { "components": { "NameComponent": { "name": "A" } } } ]
	})");

	ASSERT_TRUE(scene.loadScene(path));
	EXPECT_TRUE(findByName("A").hasComponent<TransformComponent>());
}
