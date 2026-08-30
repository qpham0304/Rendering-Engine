#pragma once

#include "core/resources/managers/Manager.h"
#include "core/features/Mesh.h"

enum class BodyType {
	None = 0,
	Box, Sphere, Capsule, Plane
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

	virtual uint32_t createBody(const Mesh& mesh, const glm::vec3& pos, const glm::vec3& scale, bool isStatic) = 0;

protected:
	

};