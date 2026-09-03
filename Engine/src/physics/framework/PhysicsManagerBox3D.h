#pragma once

#include "physics/PhysicsManager.h"
#include <box3d/box3d.h>
class PhysicsManagerBox3D : public PhysicsManager
{
public:
	PhysicsManagerBox3D(std::string serviceName = "PhysicsManager");	
	~PhysicsManagerBox3D();

	virtual bool init(WindowConfig config) override;
    virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
    virtual void onUpdate() override;

	virtual uint32_t createMeshBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, uint32_t colliderType) override;
	virtual uint32_t createBoxBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, uint32_t colliderType) override;
	virtual uint32_t createSphereBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, float radius, uint32_t colliderType) override;
	virtual uint32_t createCapsuleBody(uint32_t entityID, const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, float radius, const glm::vec3& center1, const glm::vec3& center2, uint32_t colliderType) override;
	virtual uint32_t createConvexBody() override;
	
	virtual void addForce(uint32_t id, const glm::vec3& force) override;
	virtual uint32_t rayCastClosest(const glm::vec3& location) override;
	virtual void setBullet(uint32_t id, bool flag) override;
	virtual bool isBullet(uint32_t id) override;
	virtual void enableSleep(uint32_t id, bool flag) override;
	virtual bool isSleepEnabled(uint32_t id) override;
	virtual void setAwake(uint32_t id, bool flag) override;
	virtual bool isAwake(uint32_t) override;
	virtual void setMass(uint32_t id, MassData data) override;
	virtual MassData getMass(uint32_t id) override;
	virtual void disable(uint32_t id) override;
	virtual void enable(uint32_t id) override;
	virtual bool isEnabled(uint32_t id) override;
	// virtual void setMotionLocks(uint32_t id, b3MotionLocks locks) override;
	// virtual void getMotionLocks(uint32_t id) override;
	virtual void addImpulse(uint32_t id, const glm::vec3& impulse) override;


private:
	b3WorldId m_worldId{};
	b3WorldDef m_worldDef{};
	std::unordered_map<uint32_t, b3ShapeId> m_shapeIDs;

	b3BodyId _getBody(uint32_t id);	
};