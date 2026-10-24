#include <gtest/gtest.h>
#include "core/components/MComponent.h"

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
	// from_json calls onDeserialized(), so the matrix must reflect the loaded fields without a manual update
	nlohmann::json j = { { "translateVec", { 4.0f, 5.0f, 6.0f } } };
	TransformComponent loaded = j.get<TransformComponent>();

	const glm::mat4& m = loaded.getModelMatrix();
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

TEST(SpriteComponent, ZeroRowsOrColsAreClampedOnLoad)
{
	GTEST_SKIP() << "Known bug #2 in notes/ASSET_LOADING_BUGS.md: numRows/numCols of 0 reach a division in SpriteAnimator. "
					"Remove this skip once SpriteComponent::onDeserialized clamps them.";

	nlohmann::json j = { { "numRows", 0 }, { "numCols", 0 } };
	SpriteComponent sprite = j.get<SpriteComponent>();

	EXPECT_GE(sprite.numRows, 1);
	EXPECT_GE(sprite.numCols, 1);
}
