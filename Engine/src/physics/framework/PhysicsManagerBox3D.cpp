#include "PhysicsManagerBox3D.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/components/MComponent.h"
#include "core/entities/Entity.h"
#include "window/AppWindow.h"

PhysicsManagerBox3D::PhysicsManagerBox3D(std::string serviceName)
    : PhysicsManager(serviceName)
{
	EventManager& eventManager = EventManager::getInstance();

    eventManager.subscribe(EventType::KeyPressed, [&](Event& event) {
        KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
        int keyCode = keyPressedEvent.keyCode;
    
		if(keyCode == KEY_5) {
            SceneManager& sceneManager = SceneManager::getInstance();
            for(uint32_t sceneID : sceneManager.listIDs()) {
                Scene* scene = sceneManager.getScene(sceneID);

                auto func = std::function<void(Entity)>([&](Entity entity) -> void {
                    auto& collider = entity.getComponent<ColliderComponent>();
                    auto& transform = entity.getComponent<TransformComponent>();

                    if(!collider.isStatic) {
                        transform.translate(glm::vec3(0.0,5.0,0.0));
                    }
                });

                scene->forEnitiesWith<ColliderComponent>(func);
            }
        }
    });
}

PhysicsManagerBox3D::~PhysicsManagerBox3D()
{
    
}

bool PhysicsManagerBox3D::init(WindowConfig config)
{
    Service::init(config);

    m_worldDef = b3DefaultWorldDef();
    m_worldDef.gravity = {0.0f, -9.81f, 0.0f};
    m_worldId = b3CreateWorld(&m_worldDef);

    m_logger->warn("Success! Box3D world created successfully.");


    return true;
}

bool PhysicsManagerBox3D::onClose()
{
    b3DestroyWorld(m_worldId);
    return true;
}

void PhysicsManagerBox3D::destroy(uint32_t id)
{
    
}

std::vector<uint32_t> PhysicsManagerBox3D::listIDs() const
{
    return std::vector<uint32_t>();
}

void PhysicsManagerBox3D::onUpdate()
{
    // m_logger->info("updating physics");
	float timeStep = 1.0f / 60.0f; 
    b3World_Step(m_worldId, timeStep, 4);

    SceneManager& sceneManager = SceneManager::getInstance();
    for(uint32_t sceneID : sceneManager.listIDs()) {
        Scene* scene = sceneManager.getScene(sceneID);

        auto func = std::function<void(Entity)>([&](Entity entity) -> void {
            auto& collider = entity.getComponent<ColliderComponent>();
            auto& transform = entity.getComponent<TransformComponent>();

            if(collider.shapeID == 0) {
                return;
            }

            b3ShapeId shapeId = m_shapeIDs.at(collider.shapeID);
            b3BodyId bodyId = b3Shape_GetBody(shapeId);

            b3Vec3 physicsPos = b3Body_GetPosition(bodyId);
            transform.translate({physicsPos.x, physicsPos.y, physicsPos.z});

            b3Quat physicsRot = b3Body_GetRotation(bodyId);
            glm::quat glmRotation(physicsRot.s, physicsRot.v.x, physicsRot.v.y, physicsRot.v.z);
            glm::vec3 eulerAngles = glm::eulerAngles(glmRotation);
            transform.rotate({eulerAngles.x, eulerAngles.y, eulerAngles.z});
        });

        scene->forEnitiesWith<ColliderComponent>(func);
    }
}

uint32_t PhysicsManagerBox3D::createBody(const Mesh &mesh, bool isStatic)
{
    const float min = std::numeric_limits<float>::max();
    const float max = -std::numeric_limits<float>::max();
    b3Vec3 minBounds = {  min,  min,  min };
    b3Vec3 maxBounds = { max, max, max };

    for(const auto& vertex : mesh.vertices) {
        minBounds.x = std::min(minBounds.x, vertex.positions.x);
        minBounds.y = std::min(minBounds.y, vertex.positions.y);
        minBounds.z = std::min(minBounds.z, vertex.positions.z);
        
        maxBounds.x = std::max(maxBounds.x, vertex.positions.x);
        maxBounds.y = std::max(maxBounds.y, vertex.positions.y);
        maxBounds.z = std::max(maxBounds.z, vertex.positions.z);
    }

    b3Vec3 center{};
    center.x = (minBounds.x + maxBounds.x) * 0.5;
    center.y = (minBounds.y + maxBounds.y) * 0.5;
    center.z = (minBounds.z + maxBounds.z) * 0.5;

    //Half-extents: distance from center to edges
    float hx = (maxBounds.x - minBounds.x) * 0.5;
    float hy = (maxBounds.y - minBounds.y) * 0.5;
    float hz = (maxBounds.z - minBounds.z) * 0.5;

    b3BodyDef bodyDef = b3DefaultBodyDef();
    bodyDef.type = isStatic ? b3_staticBody : b3_dynamicBody;
    bodyDef.position = center;

    b3BodyId bodyId = b3CreateBody(m_worldId, &bodyDef);

    b3ShapeDef shapeDef = b3DefaultShapeDef();
    if(!isStatic) {
        shapeDef.density = 1.0f;
    }
    
    b3BoxHull boxHull = b3MakeBoxHull(hx, hy, hz);
    b3ShapeId shapeID = b3CreateHullShape(bodyId, &shapeDef, &boxHull.base);

    if (!isStatic) {
        b3Body_ApplyMassFromShapes(bodyId);
    }

    m_shapeIDs[m_ids] = shapeID;

    return _assignID();
}
