#include <gtest/gtest.h>
#include "core/components/MComponent.h"
#include "core/entities/Entity.h"
#include "core/scene/EntitySerializer.h"

namespace {

// the scene loader's path: emplace the component from json, then run its registered load hook
void loadComponent(const std::string& type, Entity entity, const nlohmann::json& data)
{
	EntitySerializer::RegisterComponents();
	EntitySerializer::getComponentFactory().at(type)(entity, data)();
}

}

TEST(TransformComponent, JsonRoundTripKeepsValues)
{
	TransformComponent original;
	original.translateVec = { 1.0f, -2.0f, 3.5f };
	original.rotateVec = { 0.1f, 0.2f, 0.3f };
	original.scaleVec = { 2.0f, 2.0f, 0.5f };

	nlohmann::json j = original;
	TransformComponent loaded = j.get<TransformComponent>();

	EXPECT_EQ(loaded.translateVec, original.translateVec);
	EXPECT_EQ(loaded.rotateVec, original.rotateVec);
	EXPECT_EQ(loaded.scaleVec, original.scaleVec);
}

TEST(TransformComponent, LoadingRebuildsModelMatrix)
{
	// the registered load hook rebuilds the matrix, so it reflects the loaded fields without a manual update
	entt::registry registry;
	Entity entity(registry.create(), registry);
	nlohmann::json j = { { "translateVec", { 4.0f, 5.0f, 6.0f } } };
	loadComponent("TransformComponent", entity, j);

	const glm::mat4& m = entity.getComponent<TransformComponent>().getModelMatrix();
	EXPECT_FLOAT_EQ(m[3][0], 4.0f);
	EXPECT_FLOAT_EQ(m[3][1], 5.0f);
	EXPECT_FLOAT_EQ(m[3][2], 6.0f);
}

TEST(TransformComponent, MissingFieldsKeepDefaults)
{
	nlohmann::json j = nlohmann::json::object();
	TransformComponent loaded = j.get<TransformComponent>();

	EXPECT_EQ(loaded.translateVec, glm::vec3(0.0f));
	EXPECT_EQ(loaded.scaleVec, glm::vec3(1.0f));
}

TEST(TransformComponent, WrongValueTypeThrows)
{
	// EntitySerializer relies on this throwing so it can log and skip the component
	nlohmann::json j = { { "translateVec", "not a vector" } };
	EXPECT_THROW(j.get<TransformComponent>(), nlohmann::json::exception);
}

TEST(SpriteComponent, SetFrameInsideSheetIsApplied)
{
	SpriteComponent sprite;
	sprite.numRows = 2;
	sprite.numCols = 3;

	sprite.setFrame(5);
	EXPECT_EQ(sprite.frameIndex, 5);
}

TEST(SpriteComponent, SetFramePastSheetIsIgnored)
{
	SpriteComponent sprite;
	sprite.numRows = 2;
	sprite.numCols = 3;
	sprite.frameIndex = 1;

	sprite.setFrame(6);
	EXPECT_EQ(sprite.frameIndex, 1);
}

// components stay plain data, a 0 rows/cols sheet (#2) is handled by SpriteAnimator, see AssetFailureSmokeTests
TEST(SpriteComponent, LoadKeepsValuesAsWritten)
{
	nlohmann::json j = { { "numRows", 0 }, { "numCols", 0 }, { "frameIndex", 5 } };
	SpriteComponent sprite = j.get<SpriteComponent>();

	EXPECT_EQ(sprite.numRows, 0);
	EXPECT_EQ(sprite.numCols, 0);
	EXPECT_EQ(sprite.frameIndex, 5);
}

TEST(LightProbeComponent, LoadingRebuildsTheProbeGrid)
{
	LightProbeComponent original;
	original.probeCounts = { 3, 2, 4 };
	original.spacing = 2.0f;
	original.gridOrigin = { -2.0f, 0.0f, -3.0f, 1.0f };
	original.probeGrid.resize(24);

	nlohmann::json j = original;
	EXPECT_FALSE(j.contains("probeGrid"));

	entt::registry registry;
	Entity entity(registry.create(), registry);
	loadComponent("LightProbeComponent", entity, j);

	const LightProbeComponent& loaded = entity.getComponent<LightProbeComponent>();
	EXPECT_EQ(loaded.probeCounts, original.probeCounts);
	EXPECT_EQ(loaded.gridOrigin, original.gridOrigin);

	// centered on the entity: (count - 1) * spacing / 2 on each side
	ASSERT_EQ(loaded.probeGrid.size(), 3u * 2u * 4u);
	EXPECT_EQ(loaded.probeGrid.front(), glm::vec4(-2.0f, -1.0f, -3.0f, 1.0f));
	EXPECT_EQ(loaded.probeGrid.back(), glm::vec4(2.0f, 1.0f, 3.0f, 1.0f));
	EXPECT_EQ(loaded.bufferSize, 3u * 2u * 4u * sizeof(glm::vec4));
}
