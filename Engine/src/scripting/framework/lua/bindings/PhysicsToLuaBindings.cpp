#include "PhysicsToLuaBindings.h"
#include "physics/PhysicsManager.h"

void PhysicsToLuaBindings::exportPhysics(sol::state &luaState, PhysicsManager* physicsManager)
{
    _exportTypes(luaState);
    _exportBodyFunctions(luaState, physicsManager);
}

void PhysicsToLuaBindings::_exportTypes(sol::state& luaState)
{
    luaState.new_usertype<MotionLocks>("MotionLocks",
        sol::call_constructor, sol::constructors<MotionLocks()>(),
        "linearX", &MotionLocks::linearX,
        "linearY", &MotionLocks::linearY,
        "linearZ", &MotionLocks::linearZ,
        "angularX", &MotionLocks::angularX,
        "angularY", &MotionLocks::angularY,
        "angularZ", &MotionLocks::angularZ
    );

    luaState.new_enum("ColliderType",
        "Static", ColliderType::Static,
        "Kinematic", ColliderType::Kinematic,
        "Dynamic", ColliderType::Dynamic
    );
}


void PhysicsToLuaBindings::_exportBodyFunctions(sol::state& luaState, PhysicsManager* physicsManager)
{
    sol::table physics = luaState.create_named_table("physics");

    physics["addForce"] = [physicsManager](uint32_t id, const glm::vec3& force) {
        physicsManager->addForce(id, force);
    };
    physics["addImpulse"] = [physicsManager](uint32_t id, const glm::vec3& impulse) {
        physicsManager->addImpulse(id, impulse);
    };

    physics["setLinearVelocity"] = [physicsManager](uint32_t id, const glm::vec3& velocity) {
        physicsManager->setLinearVelocity(id, velocity);
    };
    physics["getLinearVelocity"] = [physicsManager](uint32_t id) {
        return physicsManager->getLinearVelocity(id);
    };
    
    physics["setAngularVelocity"] = [physicsManager](uint32_t id, const glm::vec3& velocity) {
        physicsManager->setAngularVelocity(id, velocity);
    };
    physics["getAngularVelocity"] = [physicsManager](uint32_t id) {
        return physicsManager->getAngularVelocity(id);
    };

    physics["getMass"] = [physicsManager](uint32_t id) {
        return physicsManager->getMass(id).mass;
    };

    physics["setMotionLocks"] = [physicsManager](uint32_t id, const MotionLocks& locks) {
        physicsManager->setMotionLocks(id, locks);
    };
    physics["getMotionLocks"] = [physicsManager](uint32_t id) {
        return physicsManager->getMotionLocks(id);
    };
    
    physics["lockRotation"] = sol::overload(
        [physicsManager](uint32_t id, bool locked) {
            MotionLocks locks = physicsManager->getMotionLocks(id);
            locks.angularX = locks.angularY = locks.angularZ = locked;
            physicsManager->setMotionLocks(id, locks);
        },
        [physicsManager](uint32_t id, bool x, bool y, bool z) {
            MotionLocks locks = physicsManager->getMotionLocks(id);
            locks.angularX = x;
            locks.angularY = y;
            locks.angularZ = z;
            physicsManager->setMotionLocks(id, locks);
        }
    );
    
    physics["lockPosition"] = sol::overload(
        [physicsManager](uint32_t id, bool locked) {
            MotionLocks locks = physicsManager->getMotionLocks(id);
            locks.linearX = locks.linearY = locks.linearZ = locked;
            physicsManager->setMotionLocks(id, locks);
        },
        [physicsManager](uint32_t id, bool x, bool y, bool z) {
            MotionLocks locks = physicsManager->getMotionLocks(id);
            locks.linearX = x;
            locks.linearY = y;
            locks.linearZ = z;
            physicsManager->setMotionLocks(id, locks);
        }
    );

    physics["setAwake"] = [physicsManager](uint32_t id, bool awake) { physicsManager->setAwake(id, awake); };
    physics["isAwake"] = [physicsManager](uint32_t id) { return physicsManager->isAwake(id); };
    physics["setBullet"] = [physicsManager](uint32_t id, bool bullet) { physicsManager->setBullet(id, bullet); };
    physics["enable"] = [physicsManager](uint32_t id) { physicsManager->enable(id); };
    physics["disable"] = [physicsManager](uint32_t id) { physicsManager->disable(id); };
    physics["isEnabled"] = [physicsManager](uint32_t id) { return physicsManager->isEnabled(id); };

    // closest collider under a screen point in normalized device coordinates (-1..1), returns the entity id or 0
    physics["rayCastClosest"] = [physicsManager](const glm::vec2& coordsNDC) {
        return physicsManager->rayCastClosest(coordsNDC);
    };
}
