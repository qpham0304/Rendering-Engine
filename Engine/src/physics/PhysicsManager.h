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

class PhysicsManager : public Manager
{
public:
	PhysicsManager(std::string serviceName = "PhysicsManager") : Manager(serviceName) {};	
	virtual ~PhysicsManager() = default;

	virtual bool init(WindowConfig config) = 0;
    virtual bool onClose() = 0;
	virtual void destroy(uint32_t id) = 0;
	virtual std::vector<uint32_t> listIDs() const = 0;
    virtual void onUpdate() = 0;

	virtual uint32_t createMeshBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, uint32_t colliderType) = 0;
	virtual uint32_t createBoxBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, uint32_t colliderType) = 0;
	virtual uint32_t createSphereBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, float radius, uint32_t colliderType) = 0;
	virtual uint32_t createCapsuleBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, float radius, const glm::vec3& center1, const glm::vec3& center2, uint32_t colliderType) = 0;
	virtual uint32_t createConvexBody() = 0;
	
	virtual void addForce(uint32_t id, const glm::vec3& force) = 0;
	virtual uint32_t rayCastClosest(const glm::vec3& location) = 0;
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
	// virtual void setAngularVelocity(uint32_t id, const glm::vec3& force) = 0;
	// virtual void setLinearVelocity(uint32_t id, const glm::vec3& force) = 0;
	virtual void addImpulse(uint32_t id, const glm::vec3& impulse) = 0;



	// virtual void setMotionLocks(uint32_t id, b3MotionLocks locks) = 0;
	// virtual void getMotionLocks(uint32_t) = 0;

		// b3Body_SetBullet(myBodyId, true);
		// bool isBullet = b3Body_IsBullet(myBodyId);
		// b3Body_EnableSleep(myBodyId, false);
		// bool isSleepEnabled = b3Body_IsSleepEnabled(myBodyId);
		// b3Body_SetAwake(myBodyId, true);
		// bool isAwake = b3Body_IsAwake(myBodyId);
		// b3Body_Disable(myBodyId);
		// b3Body_Enable(myBodyId);
		// bool isEnabled = b3Body_IsEnabled(myBodyId);
		// b3Body_SetMotionLocks(myBodyId, locks);
		// b3MotionLocks locks = b3Body_GetMotionLocks(myBodyId);

protected:

};