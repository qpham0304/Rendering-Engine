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

	virtual uint32_t createBody(const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, bool isStatic) override;


private:
	b3WorldId m_worldId{};
	b3WorldDef m_worldDef{};
	std::unordered_map<uint32_t, b3ShapeId> m_shapeIDs;
	// std::unordered_map<std::string, uint32_t> m_shape;
};