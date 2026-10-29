#include <gtest/gtest.h>
#include "core/features/Math.h"

namespace {

	constexpr float kEps = 1e-5f;

	void expectQuatNear(const Math::Quat& a, const Math::Quat& b, float eps = kEps)
	{
		EXPECT_NEAR(a.x, b.x, eps);
		EXPECT_NEAR(a.y, b.y, eps);
		EXPECT_NEAR(a.z, b.z, eps);
		EXPECT_NEAR(a.w, b.w, eps);
	}

	void expectVecNear(const glm::vec3& a, const glm::vec3& b, float eps = kEps)
	{
		EXPECT_NEAR(a.x, b.x, eps);
		EXPECT_NEAR(a.y, b.y, eps);
		EXPECT_NEAR(a.z, b.z, eps);
	}

	Math::Quat fromAxisAngle(const glm::vec3& axis, float radians)
	{
		Math::Quat q;
		q.fromGlmQuat(glm::angleAxis(radians, glm::normalize(axis)));
		return q;
	}

}


TEST(MathQuat, IdentityTimesQuatIsUnchanged)
{
	Math::Quat identity;
	Math::Quat q = fromAxisAngle({ 0.3f, 1.0f, -0.5f }, 1.2f);

	expectQuatNear(identity * q, q);
	expectQuatNear(q * identity, q);
}

TEST(MathQuat, NormalizeGivesUnitLength)
{
	Math::Quat q{ 1.0f, 2.0f, 3.0f, 4.0f };
	EXPECT_NEAR(Math::length(Math::normalize(q)), 1.0f, kEps);
}

TEST(MathQuat, NormalizeOfZeroDoesNotProduceNaN)
{
	Math::Quat zero{ 0.0f, 0.0f, 0.0f, 0.0f };
	Math::Quat result = Math::normalize(zero);

	EXPECT_FALSE(std::isnan(result.x));
	EXPECT_FALSE(std::isnan(result.w));
}

TEST(MathQuat, QuatTimesInverseIsIdentity)
{
	Math::Quat q = fromAxisAngle({ 1.0f, 0.0f, 0.0f }, 0.7f);
	expectQuatNear(q * Math::inverse(q), Math::Quat{});
}

TEST(MathQuat, MultiplyMatchesGlm)
{
	glm::quat ga = glm::angleAxis(0.4f, glm::normalize(glm::vec3(1, 2, 3)));
	glm::quat gb = glm::angleAxis(-1.1f, glm::normalize(glm::vec3(0, 1, 0)));
	Math::Quat a, b, expected;
	a.fromGlmQuat(ga);
	b.fromGlmQuat(gb);
	expected.fromGlmQuat(ga * gb);

	expectQuatNear(a * b, expected);
}

TEST(MathQuat, ToMat4MatchesGlm)
{
	glm::quat g = glm::angleAxis(0.9f, glm::normalize(glm::vec3(-1, 0.5f, 2)));
	Math::Quat q;
	q.fromGlmQuat(g);

	glm::mat4 ours = q.toMat4();
	glm::mat4 theirs = glm::toMat4(g);
	for (int c = 0; c < 4; c++) {
		for (int r = 0; r < 4; r++) {
			EXPECT_NEAR(ours[c][r], theirs[c][r], kEps) << "column " << c << " row " << r;
		}
	}
}

TEST(MathInterpolation, SlerpHitsEndpoints)
{
	Math::Quat a = fromAxisAngle({ 0, 1, 0 }, 0.0f);
	Math::Quat b = fromAxisAngle({ 0, 1, 0 }, 1.5f);

	expectQuatNear(Math::slerp(a, b, 0.0f), a);
	expectQuatNear(Math::slerp(a, b, 1.0f), b);
}

TEST(MathInterpolation, SlerpHalfwayIsHalfTheAngle)
{
	Math::Quat a = fromAxisAngle({ 0, 1, 0 }, 0.0f);
	Math::Quat b = fromAxisAngle({ 0, 1, 0 }, 2.0f);

	expectQuatNear(Math::slerp(a, b, 0.5f), fromAxisAngle({ 0, 1, 0 }, 1.0f));
}

TEST(MathInterpolation, SlerpTakesShortestPath)
{
	// q and -q are the same rotation, slerp must not spin the long way round
	Math::Quat a = fromAxisAngle({ 0, 0, 1 }, 0.2f);
	Math::Quat b = fromAxisAngle({ 0, 0, 1 }, 0.6f);
	Math::Quat negB{ -b.x, -b.y, -b.z, -b.w };

	Math::Quat viaB = Math::slerp(a, b, 0.5f);
	Math::Quat viaNegB = Math::slerp(a, negB, 0.5f);
	EXPECT_NEAR(std::abs(Math::dot(viaB, viaNegB)), 1.0f, kEps);
}

TEST(MathInterpolation, NlerpIsNormalized)
{
	Math::Quat a = fromAxisAngle({ 1, 0, 0 }, 0.1f);
	Math::Quat b = fromAxisAngle({ 0, 1, 0 }, 2.5f);
	EXPECT_NEAR(Math::length(Math::nlerp(a, b, 0.37f)), 1.0f, kEps);
}

TEST(MathInterpolation, LerpVec3)
{
	expectVecNear(Math::lerp({ 0, 0, 0 }, { 10, -4, 2 }, 0.25f), { 2.5f, -1.0f, 0.5f });
}

TEST(MathInterpolation, LerpHalfway)
{
	glm::vec3 result = Math::lerp({ 0, 0, 0 }, { 10, 5, 4 }, 0.5f);
	EXPECT_FLOAT_EQ(result.x, 5.0f);
	EXPECT_FLOAT_EQ(result.y, 2.5f);
	EXPECT_FLOAT_EQ(result.z, 2.0f);
}

TEST(MathCurves, CatmullRomPassesThroughMiddlePoints)
{
	glm::vec3 p0{ -1, 0, 0 }, p1{ 0, 1, 0 }, p2{ 1, 3, 0 }, p3{ 2, 2, 5 };

	expectVecNear(Math::catmullRom(p0, p1, p2, p3, 0.0f), p1);
	expectVecNear(Math::catmullRom(p0, p1, p2, p3, 1.0f), p2);
}

TEST(MathCurves, OrthonormalBasisIsPerpendicularAndUnit)
{
	// includes a near vertical axis, which takes the other helper vector branch
	for (glm::vec3 axis : { glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::normalize(glm::vec3(0.01f, 1, 0)), glm::normalize(glm::vec3(1, 2, 3)) }) {
		glm::vec3 a, b;
		Math::orthonormalBasis(axis, a, b);

		EXPECT_NEAR(glm::length(a), 1.0f, kEps);
		EXPECT_NEAR(glm::length(b), 1.0f, kEps);
		EXPECT_NEAR(glm::dot(axis, a), 0.0f, kEps);
		EXPECT_NEAR(glm::dot(axis, b), 0.0f, kEps);
		EXPECT_NEAR(glm::dot(a, b), 0.0f, kEps);
	}
}
