#include <gtest/gtest.h>
#include "TestHelpers.h"
#include "core/scene/Scene.h"
#include "core/entities/Entity.h"
#include "core/components/MComponent.h"
#include "core/events/EventManager.h"
#include "window/InputKeyCode.h"

// Adding, removing and saving entities on a Scene, no GPU involved.

namespace {

class SceneEntityTest : public ::testing::Test {
protected:
	TestHelpers::TempDir temp;
	Scene scene{ "test-scene" };

	// adds and removes are deferred until the next scene update
	void flush() { scene.onUpdate(0.0f); }
};

}

TEST_F(SceneEntityTest, AddEntityHasDefaultComponents)
{
	uint32_t id = scene.addEntity("Player");
	flush();

	ASSERT_TRUE(scene.hasEntity(id));
	Entity e = scene.getEntity(id);
	EXPECT_TRUE(e.hasComponent<TransformComponent>());
	EXPECT_TRUE(e.hasComponent<RelationshipComponent>());
	EXPECT_EQ(e.getComponent<NameComponent>().name, "Player");
}

TEST_F(SceneEntityTest, AddedEntityIsSelected)
{
	uint32_t id = scene.addEntity();
	ASSERT_EQ(scene.getSelectedEntities().size(), 1u);
	EXPECT_EQ(scene.getSelectedEntities()[0].getID(), id);
}

TEST_F(SceneEntityTest, NewEntityIsReachableBeforeFlush)
{
	uint32_t id = scene.addEntity("Early");
	EXPECT_FALSE(scene.hasEntity(id));
	EXPECT_EQ(scene.getEntity(id).getComponent<NameComponent>().name, "Early");
}

TEST_F(SceneEntityTest, RemoveEntityTakesEffectOnUpdate)
{
	uint32_t id = scene.addEntity();
	flush();

	EXPECT_TRUE(scene.removeEntity(id));
	EXPECT_TRUE(scene.hasEntity(id));	// still there until the update
	flush();
	EXPECT_FALSE(scene.hasEntity(id));
	EXPECT_TRUE(scene.getSelectedEntities().empty());
}

TEST_F(SceneEntityTest, RemoveByName)
{
	uint32_t id = scene.addEntity("Target");
	scene.addEntity("Other");
	flush();

	EXPECT_TRUE(scene.removeEntity(std::string("Target")));
	flush();
	EXPECT_FALSE(scene.hasEntity(id));
	EXPECT_FALSE(scene.removeEntity(std::string("Nobody")));
}

TEST_F(SceneEntityTest, GetUnknownEntityThrows)
{
	// documents current behaviour, bug #15 suggests returning an invalid Entity or std::optional instead
	EXPECT_THROW(scene.getEntity(123456u), std::runtime_error);
}

TEST_F(SceneEntityTest, DuplicateCopiesComponentsAndOffsetsPosition)
{
	uint32_t id = scene.addEntity("Original");
	flush();
	Entity original = scene.getEntity(id);
	original.getComponent<TransformComponent>().translate({ 1.0f, 2.0f, 3.0f });

	uint32_t copyID = scene.duplicateEntity(original);
	Entity copy(static_cast<entt::entity>(copyID), *original.getRegistry());

	EXPECT_EQ(copy.getComponent<NameComponent>().name, "Original");
	EXPECT_FLOAT_EQ(copy.getComponent<TransformComponent>().translateVec.x, 1.25f);
	EXPECT_FLOAT_EQ(copy.getComponent<TransformComponent>().translateVec.y, 2.0f);
}

TEST_F(SceneEntityTest, SaveThenLoadRoundTrip)
{
	uint32_t id = scene.addEntity("Saved");
	flush();
	scene.getEntity(id).getComponent<TransformComponent>().translate({ 7.0f, 8.0f, 9.0f });

	std::string path = temp.file("roundtrip.json");
	ASSERT_TRUE(scene.saveScene(path));

	Scene loaded{ "loaded-scene" };
	ASSERT_TRUE(loaded.loadScene(path));

	auto entities = loaded.getEntitiesWith<NameComponent, TransformComponent>();
	ASSERT_EQ(entities.size(), 1u);
	EXPECT_EQ(entities[0].getComponent<NameComponent>().name, "Saved");
	EXPECT_EQ(entities[0].getComponent<TransformComponent>().translateVec, glm::vec3(7.0f, 8.0f, 9.0f));
}

TEST_F(SceneEntityTest, SavedJsonHasSceneNameAndEntities)
{
	scene.addEntity("A");
	scene.addEntity("B");
	flush();

	std::string path = temp.file("out.json");
	ASSERT_TRUE(scene.saveScene(path));

	std::ifstream in(path);
	nlohmann::json j = nlohmann::json::parse(in);
	EXPECT_EQ(j["scene_name"], "test-scene");
	EXPECT_EQ(j["entities"].size(), 2u);
}

// notes/ASSET_LOADING_BUGS.md #15: Ctrl+D with nothing selected read selectedEntities[0] from an empty vector
TEST_F(SceneEntityTest, DuplicateShortcutWithNothingSelectedDoesNothing)
{
	scene.addEntity("Only");
	flush();
	scene.selectEntities({});
	size_t before = scene.getEntitiesWith<TransformComponent>().size();

	KeyPressedEvent control(KEY_LEFT_CONTROL);
	KeyPressedEvent duplicate(KEY_D);
	EventManager::getInstance().publish(control);
	EventManager::getInstance().publish(duplicate);

	EXPECT_EQ(scene.getEntitiesWith<TransformComponent>().size(), before);
}
