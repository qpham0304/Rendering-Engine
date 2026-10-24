#pragma once

#include "core/resources/managers/Manager.h"
#include "core/features/Mesh.h"

enum class BodyType {
	None = 0,
	Box, Sphere, Capsule, Convex
};

enum class ColliderType {
	Static = 0,
	Kinematic = 1,
	Dynamic = 2
};

struct MassData {
	float mass;
	glm::vec3 center;		// The local center of mass position.
	glm::mat3 inertia;		// The inertia tensor about the shape center of mass.
};

// movement axis lock
struct MotionLocks {
	bool linearX { false };
	bool linearY { false };
	bool linearZ { false };
	bool angularX { false };
	bool angularY { false };
	bool angularZ { false };
};

class MYGRAPHICSENGINE_EXPORT PhysicsManager : public Manager
{
public:
	PhysicsManager(std::string serviceName = "PhysicsManager") : Manager(serviceName) { setFootprint("PhysicsManager", sizeof(PhysicsManager), MemorySystem::Physics); };	
	virtual ~PhysicsManager() = default;

	virtual bool init(WindowConfig config) = 0;
    virtual bool onClose() = 0;
	virtual void destroy(uint32_t id) = 0;
	virtual std::vector<uint32_t> listIDs() const = 0;
    virtual void onFixedUpdate(float fixedDeltaTime) override = 0;
    virtual void onUpdate(const FrameTime& frameTime) override = 0;

	virtual uint32_t createMeshBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, uint32_t colliderType) = 0;
	virtual uint32_t createBoxBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, uint32_t colliderType) = 0;
	virtual uint32_t createSphereBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, float radius, uint32_t colliderType) = 0;
	virtual uint32_t createCapsuleBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, float radius, const glm::vec3& center1, const glm::vec3& center2, uint32_t colliderType) = 0;
	virtual uint32_t createConvexBody() = 0;
	
	virtual void addForce(uint32_t id, const glm::vec3& force) = 0;
	virtual uint32_t rayCastClosest(const glm::vec2& coordsNDC) = 0;
	virtual std::vector<glm::vec3> getColliderTriangles(uint32_t id) = 0;	// body space triangle list of the collider shape
	virtual glm::mat4 getColliderTransform(uint32_t id) = 0;				// body to world, pairs with getColliderTriangles
	virtual void setBullet(uint32_t id, bool flag) = 0;
	virtual bool isBullet(uint32_t id) = 0;
	virtual void enableSleep(uint32_t id, bool flag) = 0;
	virtual bool isSleepEnabled(uint32_t id) = 0;
	virtual void setAwake(uint32_t id, bool flag) = 0;
	virtual bool isAwake(uint32_t) = 0;
	virtual void setMass(uint32_t id, MassData data) = 0;
	virtual MassData getMass(uint32_t id) = 0;
	virtual void disable(uint32_t id) = 0;
	virtual void enable(uint32_t id) = 0;
	virtual bool isEnabled(uint32_t id) = 0;
	virtual void addImpulse(uint32_t id, const glm::vec3& impulse) = 0;
	virtual void setLinearVelocity(uint32_t id, const glm::vec3& velocity) = 0;
	virtual glm::vec3 getLinearVelocity(uint32_t id) = 0;
	virtual void setAngularVelocity(uint32_t id, const glm::vec3& velocity) = 0;	// radians per second
	virtual glm::vec3 getAngularVelocity(uint32_t id) = 0;
	virtual void setMotionLocks(uint32_t id, const MotionLocks& locks) = 0;
	virtual MotionLocks getMotionLocks(uint32_t id) = 0;

protected:

};